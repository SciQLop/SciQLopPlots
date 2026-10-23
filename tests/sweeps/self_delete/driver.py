"""driver.py discover|pairs|summary [KIND...]: runs every subprocess sequentially under poisoned malloc.

SQP_BUILD (default <repo>/build-venv) is the build dir; SWEEP_OUT (default this dir) gets the jsonl files.
"""
import collections
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
PY = sys.executable
BUILD = os.environ.get("SQP_BUILD", os.path.join(REPO, "build-venv"))
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, BUILD)  # common imports SciQLopPlots for the kind list

import common as C  # noqa: E402
OUT = os.environ.get("SWEEP_OUT", HERE)
ENV = dict(os.environ, GLIBC_TUNABLES="glibc.malloc.tcache_count=0", MALLOC_PERTURB_="165",
           QT_QPA_PLATFORM="offscreen", PYTHONPATH=f"{BUILD}:{HERE}")
DISC = os.path.join(OUT, "discovery.jsonl")
PAIRS = os.path.join(OUT, "pairs.jsonl")


def run(args, timeout):
    try:
        p = subprocess.run([PY, *args], cwd=HERE, env=ENV, capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired as e:
        dec = lambda b: b.decode(errors="replace") if isinstance(b, bytes) else (b or "")
        return "timeout", dec(e.stdout), dec(e.stderr)


def lines(path):
    if not os.path.exists(path):
        return []
    return [json.loads(l) for l in open(path)]


def discover(kinds):
    for kind in kinds:
        start = 0
        while True:
            code, out, err = run(["discover.py", kind, str(start), DISC], 300)
            if code == 0 and "ALLDONE" in out:
                break
            mine = [r for r in lines(DISC) if r["kind"] == kind]
            last = mine[-1] if mine else None
            if last is None or last["status"] != "start":
                with open(DISC, "a") as f:
                    f.write(json.dumps({"kind": kind, "status": "build_failed", "code": str(code),
                                        "err": (err or "")[-400:]}) + "\n")
                break
            with open(DISC, "a") as f:
                f.write(json.dumps({"kind": kind, "i": last["i"], "setter": last["setter"], "status": "crashed",
                                    "code": str(code), "err": (err or "")[-300:]}) + "\n")
            start = last["i"] + 1
        print(kind, "discovered", flush=True)


def pair_jobs(kinds):
    for r in lines(DISC):
        if r["status"] != "done" or r["kind"] not in kinds or r["idx"] is None:
            continue
        targets = ["self", "owner"] if r["kind"] in C.COMPONENT_OWNERS else ["self"]
        for part, sig in r["emitted"]:
            for t in targets:
                yield r["kind"], r["setter"], r["idx"], part, sig, t


def classify(code, out):
    if code == "timeout":
        return "HANG"
    if code != 0:
        return f"CRASH({code})" if "DONE" not in out else f"CRASH_AT_EXIT({code})"
    if "NOFIRE" in out:
        return "no emit"
    return "ok" + (" (raised)" if "RAISED" in out else "")


def pairs(kinds):
    done = {tuple(r["job"]) for r in lines(PAIRS)}
    with open(PAIRS, "a") as f:
        for job in pair_jobs(kinds):
            if tuple(job) in done:
                continue
            t0 = time.time()
            code, out, err = run(["run_pair.py", *map(str, job)], 20)
            res = classify(code, out)
            raised = next((l for l in out.splitlines() if l.startswith("RAISED")), "")
            f.write(json.dumps({"job": job, "result": res, "code": str(code), "raised": raised,
                                "secs": round(time.time() - t0, 1),
                                "err": (err or "")[-300:] if res.startswith(("CRASH", "HANG")) else ""}) + "\n")
            f.flush()
            if res != "ok" and res != "no emit":
                print(job, res, flush=True)


def _counts(statuses):
    counts = collections.Counter(statuses)
    return ", ".join(f"{k} {v}" for k, v in sorted(counts.items()))


def _setter_status(r):
    """One final status per probed setter; a crash record supersedes its start record."""
    # A discovery file from before the "outcome" field can't tell "no emit" from "no args".
    return r.get("outcome", "unknown (stale discovery.jsonl)") if r["status"] == "done" else r["status"]


def discovery_summary(kinds):
    final = {}
    for r in lines(DISC):
        if r["kind"] in kinds and r["status"] != "start":
            final[(r["kind"], r.get("setter", ""))] = _setter_status(r)
    print(len(final), "setters:", _counts(final.values()))
    for (kind, setter), status in sorted(final.items()):
        if status not in ("emitted", "no emit"):
            print(" ", kind, setter, status)


def summary(kinds):
    discovery_summary(kinds)
    rows = [r for r in lines(PAIRS) if r["job"][0] in kinds]
    print(len(rows), "pairs:", _counts(r["result"].split("(")[0].strip() for r in rows))
    for r in rows:
        if r["result"].startswith(("CRASH", "HANG")):
            print(" ".join(map(str, r["job"])), r["result"])


if __name__ == "__main__":
    phase, kinds = sys.argv[1], sys.argv[2:] or list(C.BUILDERS)
    {"discover": discover, "pairs": pairs, "summary": summary}[phase](kinds)

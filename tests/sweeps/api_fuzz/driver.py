"""API fuzz sweep entry points.

    python -m tests.sweeps.api_fuzz.driver sweep [--classes A,B] [--jobs 4]
    python -m tests.sweeps.api_fuzz.driver folder PATH

Run from outside the repo (cd /tmp) with PYTHONPATH=<build dir>:<repo root>: from the repo root the
source package would shadow the built one. Reports go to $SCIQLOP_PLOTS_TEST_REPORTS
(default test-reports/): api_fuzz.jsonl (one verdict per case) and api_fuzz_summary.md.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import sys
from concurrent.futures import ThreadPoolExecutor

from tests.sweeps.api_fuzz import catalogue as cat
from tests.sweeps.api_fuzz.harness import Case, classify, run_batch
from tests.sweeps.api_fuzz.mutations import cases_for

CHUNK = 40
DELETABLE = {"SciQLopPlotInterface", "SciQLopPlotPanelInterface", "SciQLopPlottableInterface",
             "SciQLopItemInterface", "SciQLopPlotGroup"}


def _run(cases: list[Case], jobs: int, chunk: int = CHUNK) -> list[dict]:
    chunks = [cases[i:i + chunk] for i in range(0, len(cases), chunk)]
    with ThreadPoolExecutor(jobs) as pool:
        outcomes = [o for batch in pool.map(run_batch, chunks) for o in batch]
    return [{"id": c.id, "status": v.status, "detail": v.detail, "expect": c.expect, "call": c.call}
            for c, v in ((c, classify(c, o)) for c, o in zip(cases, outcomes))]


def _discovery_rows(found: list[dict]) -> list[dict]:
    """Discovery probes that crashed or hung are findings too. Printed noise isn't: the probes try
    arguments that raise on purpose, and their own cases grade the noise."""
    return [{"id": f"discover:{r['kind']}:{r['cls']}.{r['name']}", "status": r["status"], "detail": r["detail"],
             "expect": "ok", "call": f"find_baseline({r['kind']!r}, {r['name']!r})"}
            for r in found if r["status"] in ("crash", "hang")]


def sweep(classes: list[str] | None = None, jobs: int = 4) -> list[dict]:
    methods = [m for m in cat.public_methods() if classes is None or m.cls in classes]
    # A static method doesn't touch its receiver: deleting it first raises nothing, rightly.
    static = {(m.cls, m.name) for m in methods if m.signatures and not any("self" in s for s in m.signatures)}
    mros = cat.receiver_mros(cat.kinds())
    found = cat.discover_all(methods, mros, jobs)
    cases, mutated = [], set()
    for r in found:
        if not r["found"]:
            continue
        key = (r["cls"], r["name"])
        if key in mutated:
            cases += cases_for(r, receiver_deletable=False)[:1]  # the baseline alone on other receivers
            continue
        mutated.add(key)
        deletable = bool(DELETABLE & set(mros[r["kind"]] or [])) and key not in static
        cases += cases_for(r, receiver_deletable=deletable)
    return _discovery_rows(found) + _run(cases, jobs)


def run_folder(path: str, jobs: int = 4) -> list[dict]:
    """Each script in `path` is one case, in its own child: it must return without printing errors."""
    scripts = sorted(f for f in os.listdir(path) if f.endswith(".py"))
    cases = [Case(f, "", open(os.path.join(path, f)).read(), "script") for f in scripts]
    return _run(cases, jobs, chunk=1)


def summary(rows: list[dict]) -> str:
    counts = collections.Counter(r["status"] for r in rows)
    lines = ["# API fuzz summary", "", *[f"- {s}: {n}" for s, n in counts.most_common()], ""]
    failing = collections.defaultdict(list)
    for r in rows:
        if r["status"] != "pass":
            failing[(r["id"].split(":")[1] if ":" in r["id"] else r["id"], r["status"])].append(r)
    for (method, status), rs in sorted(failing.items()):
        lines.append(f"## {method} — {status} ({len(rs)})")
        lines += [f"- `{r['id']}`: {r['detail'].splitlines()[0][:200] if r['detail'] else ''}" for r in rs[:5]]
        lines.append("")
    return "\n".join(lines)


def write_reports(rows: list[dict], out_dir: str) -> None:
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "api_fuzz.jsonl"), "w") as f:
        f.writelines(json.dumps(r) + "\n" for r in rows)
    with open(os.path.join(out_dir, "api_fuzz_summary.md"), "w") as f:
        f.write(summary(rows))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    s = sub.add_parser("sweep")
    s.add_argument("--classes", default="")
    s.add_argument("--jobs", type=int, default=4)
    f = sub.add_parser("folder")
    f.add_argument("path")
    f.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args(argv)
    if args.mode == "sweep":
        rows = sweep([c for c in args.classes.split(",") if c] or None, args.jobs)
    else:
        rows = run_folder(args.path, args.jobs)
    write_reports(rows, os.environ.get("SCIQLOP_PLOTS_TEST_REPORTS", "test-reports"))
    print(summary(rows).split("\n## ")[0])
    return 0 if all(r["status"] == "pass" for r in rows) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

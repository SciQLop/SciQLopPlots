"""Mutant runs: apply one mutant, build, run the tests that cover its line, record, restore.

Every step is saved to the state file, so a run stopped by its budget, a signal
or SIGKILL continues where it stopped. A mutant is a survivor only after every
covering test ran against it, slice by slice when `max_tests` caps a pass.
"""
import dataclasses
import shutil
import subprocess
import time
from dataclasses import asdict

import covermap
import operators as ops
import outcomes
import state as st
from config import target_lines

DONE = ("killed", "survived", "timeout", "build_error", "equivalent", "interrupted")
MAX_ATTEMPTS = 2


def covering_tests(tests):
    out = {}
    for name, entry in tests.items():
        if entry.get("status") != "passed":
            continue
        for path, lines in entry.get("lines", {}).items():
            for line in lines:
                out.setdefault(path, {}).setdefault(line, []).append(name)
    return out


def _by_speed(names, tests):
    return sorted(names, key=lambda n: tests[n].get("seconds", 0))


def _merge(old, fresh, equivalent):
    if old is None:
        return {**fresh, "status": "equivalent" if equivalent else "pending", "killed_by": [], "ran": 0,
                "seconds": 0.0, "build_seconds": 0.0, "attempts": 0}
    if equivalent:
        return {**old, "status": "equivalent"}
    if old["status"] in ("pending", "survived") and set(fresh["tests"]) - set(old["tests"]):
        return {**old, **fresh, "status": "pending", "ran": 0, "attempts": 0}
    return old


def _check_clean(project, when):
    paths = [t.path for t in project.targets]
    out = subprocess.run(["git", "-C", str(project.repo), "status", "--porcelain", "--untracked-files=no", "--", *paths],
                         capture_output=True, text=True, check=True).stdout
    if out.strip():
        raise SystemExit(f"{when}: target files have uncommitted changes:\n{out}")


def _stale_equivalents(project, mutants):
    planned = {(m["path"], m["op"], m["before"]) for m in mutants.values()}
    return [dataclasses.asdict(e) for e in project.equivalents if (e.path, e.op, e.before) not in planned]


def plan(project):
    _check_clean(project, "plan")
    data = st.load(project.state_path)
    if not data["tests"] or any(e["status"] == "pending" for e in data["tests"].values()):
        raise SystemExit("cover is not finished: run cover until it reports 0 left")
    shas = covermap.source_shas(project)
    if data.get("cover_files") != shas:
        raise SystemExit("sources changed since cover: run cover --reset, then plan")
    data["mutants"] = {k: m for k, m in data["mutants"].items() if data["files"].get(m["path"]) == shas.get(m["path"])}
    data["files"] = shas
    covering = covering_tests(data["tests"])
    equivalents = {(e.path, e.op, e.before) for e in project.equivalents}
    for target in project.targets:
        lines = target_lines(project, target)
        file_cover = covering.get(target.path, {})
        covered = {line for line, names in file_cover.items() if names}
        code = {n for e in data["tests"].values() for n in e.get("code_lines", {}).get(target.path, [])}
        data["uncovered"][target.path] = sorted((lines & code) - covered)
        text = st.read_source(project.repo / target.path)
        for m in ops.mutants_for_file(target.path, text, lines & covered):
            names = file_cover[m.line]
            fresh = {**asdict(m), "tests": _by_speed(names, data["tests"]), "covering": len(names)}
            data["mutants"][m.id] = _merge(data["mutants"].get(m.id), fresh, (m.path, m.op, m.before) in equivalents)
    data["stale_equivalents"] = _stale_equivalents(project, data["mutants"])
    st.save(project.state_path, data)
    return data


def _status(killed):
    if not killed:
        return "survived"
    return "timeout" if all(k.endswith("::<timeout>") for k in killed) else "killed"


def _run_tests(project, names, baseline):
    batches = [names] if project.raw.get("batch_tests") else [[n] for n in names]
    killed, seconds = [], 0.0
    for batch in batches:
        timeout = 30 + 3 * sum(baseline.get(n, {}).get("seconds", 60) for n in batch)
        result = project.run(project.command("test_cmd", batch), cwd=project.cwd(), env=project.env(batch, mutation=True), timeout=timeout)
        shutil.rmtree(project.run_dir / "gcda-junk", ignore_errors=True)
        seconds += result.seconds
        killed += outcomes.failures(project.raw["failure_parser"], result, batch)
        if killed and project.raw.get("stop_on_first_kill"):
            break
    return killed, seconds


def _next_slice(project, entry):
    start = entry.get("ran", 0)
    size = project.raw.get("max_tests") or len(entry["tests"])
    return start, entry["tests"][start:start + size]


def run_one(project, data, entry):
    entry["attempts"] += 1
    if entry["attempts"] > MAX_ATTEMPTS:
        return {"status": "interrupted"}
    st.save(project.state_path, data)
    mutant = ops.Mutant(**{k: entry[k] for k in ("path", "line", "col", "op", "before", "after")})
    deploy = project.deploy_path(project.target(mutant.path))
    text = ops.apply(st.read_source(deploy), mutant)
    start, batch = _next_slice(project, entry)
    with st.mutated(project.state_path, data, deploy, text, project.run_dir / "backup"):
        built = project.build()
        if built.code != 0:
            return {"status": "build_error", "seconds": round(built.seconds, 1), "build_seconds": round(built.seconds, 1)}
        killed, seconds = _run_tests(project, batch, data["tests"])
    ran = start + len(batch)
    finished = bool(killed) or ran >= len(entry["tests"])
    return {"status": _status(killed) if finished else "pending", "killed_by": killed, "ran": ran,
            "attempts": entry["attempts"] if finished else 0,
            "seconds": round(built.seconds + seconds, 1), "build_seconds": round(built.seconds, 1)}


def _check_sources(project, data):
    changed = [p for p, digest in data["files"].items() if st.sha(project.repo / p) != digest]
    if changed:
        raise SystemExit(f"sources changed since plan: {changed}; run cover --reset, then plan")


def _estimate(mutants):
    timed = [m for m in mutants.values() if m.get("seconds")]
    return max((m["seconds"] for m in timed), default=120) + max((m.get("build_seconds", 0) for m in timed), default=60)


def _finish(project, data):
    covermap.build_or_die(project)
    _check_clean(project, "run")
    return sum(m["status"] == "pending" for m in data["mutants"].values())


def run(project, budget):
    data = st.load(project.state_path)
    covermap.recover(project, data)
    _check_sources(project, data)
    started = time.monotonic()
    covermap.build_or_die(project)
    for entry in data["mutants"].values():
        while entry["status"] == "pending":
            if time.monotonic() - started + _estimate(data["mutants"]) > budget:
                return _finish(project, data)
            entry.update(run_one(project, data, entry))
            st.save(project.state_path, data)
    return _finish(project, data)


def try_mutant(project, path, line, op, tests):
    """One mutant of the current source against `tests`, outside the plan: the check after a triage fix."""
    if not tests:
        raise SystemExit("try: no tests given")
    data = st.load(project.state_path)
    covermap.recover(project, data)
    covermap.build_or_die(project)
    text = st.read_source(project.repo / path)
    found = [m for m in ops.mutants_for_line(path, line, text.splitlines()[line - 1]) if m.op == op]
    if not found:
        raise SystemExit(f"no {op} mutant on {path}:{line}")
    entry = {**asdict(found[0]), "tests": list(tests), "ran": 0, "attempts": 0, "status": "pending"}
    while entry["status"] == "pending":
        entry.update(run_one(project, data, entry))
    covermap.build_or_die(project)
    return entry

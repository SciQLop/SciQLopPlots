"""Per-test coverage of the target files: which test runs which target line.

Each test runs alone from zeroed gcov counters; the configured report commands
(gcovr, coverage.py) then dump what ran. A test's entry also keeps whether it
passed, ran anything, and how long it took: the baseline the mutant runs rely on.
"""
import json
import os
import shutil
import time
from pathlib import Path

import outcomes
import proc
import state as st
from config import discover_tests

HEADERS = (".h", ".hpp", ".py")


def _gcovr(data, root):
    files = {}
    for f in data["files"]:
        code = [line for line in f["lines"] if not line.get("gcovr/noncode")]
        files[os.path.realpath(Path(root) / f["file"])] = (
            {line["line_number"] for line in code if line["count"] > 0},
            {line["line_number"] for line in code},
        )
    return files


def _coveragepy(data, root):
    return {
        os.path.realpath(Path(root) / name): (set(f["executed_lines"]), set(f["executed_lines"]) | set(f["missing_lines"]))
        for name, f in data["files"].items()
    }


READERS = {"gcovr": _gcovr, "coveragepy": _coveragepy}


def recover(project, data):
    proc.kill_leftover(project.group_file)
    return st.restore_leftovers(project.state_path, data)


def source_shas(project):
    return {t.path: st.sha(project.repo / t.path) for t in project.targets}


def build_or_die(project):
    result = project.build()
    if result.code != 0:
        raise SystemExit(f"build failed on the unmutated sources:\n{result.output[-3000:]}")


def reset_counters(build_dir):
    for gcda in Path(build_dir).rglob("*.gcda"):
        gcda.unlink()


def counter_names(project):
    """A header's code is counted in the objects that include it, so a header target names them."""
    sources = [t.path for t in project.targets if not t.path.endswith(HEADERS)] + [o for t in project.targets for o in t.objects]
    return {Path(s).name + ".gcda" for s in sources}


def prune_counters(build_dir, names):
    """Drops the counters of non-target objects, so gcovr reads a handful of files, not hundreds."""
    for gcda in Path(build_dir).rglob("*.gcda"):
        if not any(gcda.name == n or gcda.name.endswith("_" + n) for n in names):
            gcda.unlink()


def read_reports(project):
    found = {}
    for spec in project.raw.get("cover_reports", []):
        path = Path(project.expand(spec["file"]))
        if not path.exists():
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        for name, (covered, code) in READERS[spec["kind"]](data, project.expand(spec.get("root", "/"))).items():
            old_covered, old_code = found.get(name, (set(), set()))
            found[name] = (old_covered | covered, old_code | code)
    return found


def _target_files(project):
    pairs = [(project.repo / t.path, t.path) for t in project.targets] + [(project.deploy_path(t), t.path) for t in project.targets]
    return {os.path.realpath(p): name for p, name in pairs}


def _run_reports(project):
    """The report commands that failed, as 'name exited N'."""
    broken = []
    for cmd in project.raw.get("cover_cmds", []):
        argv = project.expand(cmd)
        result = project.run(argv, cwd=str(project.root), env=project.env(), timeout=300)
        if result.code != 0:
            broken.append(f"{Path(argv[0]).name} exited {result.code}")
    return broken


def _missing_reports(project):
    return [spec["file"] for spec in project.raw.get("cover_reports", []) if not Path(project.expand(spec["file"])).exists()]


def _note(project, result, broken):
    if result.timed_out:
        return "timed out"
    if result.code != 0:
        return f"exit {result.code}"
    if not outcomes.executed(project.raw["failure_parser"], result.output):
        return "ran no test"
    if broken:
        return "report failed: " + ", ".join(broken)
    return "no report" if _missing_reports(project) else ""


def cover_one(project, name):
    shutil.rmtree(project.scratch, ignore_errors=True)
    project.scratch.mkdir(parents=True)
    reset_counters(project.build_dir)
    result = project.run(project.command("cover_test_cmd", [name]), cwd=project.cwd(), env=project.env([name]),
                         timeout=project.raw.get("cover_timeout", 300))
    if project.raw.get("prune_gcda"):
        prune_counters(project.build_dir, counter_names(project))
    note = _note(project, result, _run_reports(project))
    targets = _target_files(project)
    found = {targets[f]: lines for f, lines in read_reports(project).items() if f in targets}
    return {
        "status": "timeout" if result.timed_out else "failed" if note else "passed",
        "note": note,
        "seconds": round(result.seconds, 2),
        "lines": {path: sorted(covered) for path, (covered, _) in found.items() if covered},
        "code_lines": {path: sorted(code) for path, (_, code) in found.items()},
    }


def merge_tests(old, names, reset):
    """Discovery runs on every call, so a test added or deleted since the last call is not stale."""
    return {name: old[name] if name in old and not reset else {"status": "pending"} for name in names}


def _estimate(tests):
    return 30 + max((e["seconds"] for e in tests.values() if "seconds" in e), default=60)


def cover(project, budget, reset=False):
    data = st.load(project.state_path)
    recover(project, data)
    shas = source_shas(project)
    if reset or "cover_files" not in data:
        data["cover_files"] = shas
    elif data["cover_files"] != shas:
        raise SystemExit("sources changed since cover started: run cover --reset")
    build_or_die(project)
    data["tests"] = merge_tests(data["tests"], discover_tests(project), reset)
    st.save(project.state_path, data)
    started = time.monotonic()
    for name, entry in data["tests"].items():
        if entry["status"] != "pending":
            continue
        if time.monotonic() - started + _estimate(data["tests"]) > budget:
            break
        entry.update(cover_one(project, name))
        st.save(project.state_path, data)
    return sum(e["status"] == "pending" for e in data["tests"].values())

import fcntl
import os
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

import config
import covermap
import operators as ops
import state as st
from fixture_repo import init_repo

FIXTURE = Path(__file__).parent / "fixture"
MUTATE = Path(__file__).resolve().parents[1] / "mutate.py"

CONFIG = """
root = "."

[[project]]
name = "slow"
repo = "repo"
base = "main"
build_dir = "build"
build_cmd = ["true"]
test_cmd = ["sleep", "60"]
failure_parser = "qtest"

[[project.target]]
path = "calc.cpp"

[[project]]
name = "needs"
repo = "repo"
base = "main"
build_dir = "build-needs"
require_env = ["MUTATION_TEST_NEVER_SET"]
build_cmd = ["true"]

[[project.target]]
path = "calc.cpp"
"""


def _slow(tmp_path):
    repo = tmp_path / "repo"
    shutil.copytree(FIXTURE, repo, ignore=shutil.ignore_patterns("__pycache__"))
    init_repo(repo)
    (tmp_path / "phase.toml").write_text(CONFIG)
    project = config.load(tmp_path / "phase.toml", "slow")
    mutant = next(m for m in ops.mutants_for_line("calc.cpp", 3, "    if (i < 0)") if m.op == "negate_if")
    data = st.new_state()
    data["tests"] = {"t": {"status": "passed", "seconds": 1.0}}
    data["files"] = data["cover_files"] = covermap.source_shas(project)
    data["mutants"] = {mutant.id: {"path": mutant.path, "line": mutant.line, "col": mutant.col, "op": mutant.op,
                                   "before": mutant.before, "after": mutant.after, "status": "pending", "tests": ["t"],
                                   "covering": 1, "ran": 0, "killed_by": [], "seconds": 0.0, "build_seconds": 0.0,
                                   "attempts": 0}}
    st.save(project.state_path, data)
    return project


def _cli(tmp_path, project, *args):
    return [sys.executable, str(MUTATE), "--config", str(tmp_path / "phase.toml"), "--project", project, *args]


def _wait(condition, seconds=20):
    deadline = time.monotonic() + seconds
    while not condition():
        assert time.monotonic() < deadline, "timed out waiting"
        time.sleep(0.05)


def _gone(pid, wait=5.0):
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        try:
            if Path(f"/proc/{pid}/stat").read_text().split(") ", 1)[1].startswith("Z"):
                return True
        except FileNotFoundError:
            return True
        time.sleep(0.05)
    return False


def test_sigterm_mid_mutant_restores_and_exits_3(tmp_path):
    project = _slow(tmp_path)
    source = project.repo / "calc.cpp"
    original = st.read_source(source)
    child = subprocess.Popen(_cli(tmp_path, "slow", "run", "--budget", "600"))
    _wait(lambda: st.read_source(source) != original)
    child.send_signal(signal.SIGTERM)
    assert child.wait(timeout=20) == 3
    assert st.read_source(source) == original
    data = st.load(project.state_path)
    assert data["in_flight"] == []
    entry = next(iter(data["mutants"].values()))
    assert entry["status"] == "pending" and entry["attempts"] == 1


def test_sigkill_of_the_whole_cli_group_is_recovered_by_the_next_command(tmp_path):
    project = _slow(tmp_path)
    source = project.repo / "calc.cpp"
    original = st.read_source(source)
    child = subprocess.Popen(_cli(tmp_path, "slow", "run", "--budget", "600"), start_new_session=True)
    _wait(lambda: st.read_source(source) != original and project.group_file.exists())
    time.sleep(0.5)
    sleeper = int(project.group_file.read_text())
    os.killpg(child.pid, signal.SIGKILL)
    child.wait(timeout=10)
    assert st.read_source(source) != original
    assert not _gone(sleeper, wait=0.5), "the test command should outlive the killed CLI"
    result = subprocess.run(_cli(tmp_path, "slow", "restore"), capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    assert st.read_source(source) == original
    assert _gone(sleeper)


def test_a_second_command_on_the_same_build_dir_is_refused(tmp_path):
    project = _slow(tmp_path)
    with open(project.run_dir / "lock", "w") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        result = subprocess.run(_cli(tmp_path, "slow", "report"), capture_output=True, text=True, timeout=30)
    assert result.returncode == 4
    assert "another mutation run" in result.stderr


def test_a_missing_required_env_var_stops_before_anything_runs(tmp_path):
    _slow(tmp_path)
    result = subprocess.run(_cli(tmp_path, "needs", "plan"), capture_output=True, text=True, timeout=30)
    assert result.returncode != 0
    assert "MUTATION_TEST_NEVER_SET" in result.stderr and "xvfb-run -a" in result.stderr


def test_report_writes_the_file(tmp_path):
    _slow(tmp_path)
    out = tmp_path / "r.md"
    result = subprocess.run(_cli(tmp_path, "slow", "report", "--out", str(out)), capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    assert out.read_text().startswith("# Mutation report: slow")
    assert (tmp_path / "r-kill-matrix.json").exists()

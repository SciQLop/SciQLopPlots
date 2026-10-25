import os
import shutil
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

import config
import covermap
import state as st
from fixture_repo import make_repo

FIXTURE = Path(__file__).parent / "fixture"
REPORT = 'cover_cmds = [["{env_PYTHON}", "{repo}/fake_report.py", "{scratch}"]]'

CONFIG = """
root = "."

[[project]]
name = "p"
repo = "repo"
base = "main"
build_dir = "build"
build_cmd = ["true"]
failure_parser = "qtest"
cover_test_cmd = ["{env_PYTHON}", "-c", "import sys; open(sys.argv[1] + '/ran', 'w').write(sys.argv[2]); print('' if sys.argv[2] == 'empty' else 'PASS   : Fake::' + sys.argv[2] + '()'); sys.exit(sys.argv[2] == 'bad')", "{scratch}", "{test}"]
%s
cover_reports = [{ kind = "gcovr", file = "{scratch}/gcovr.json", root = "{repo}" }]
tests = { regex_file = "repo/tests.txt", pattern = '(\\w+)' }

[[project.target]]
path = "a.cpp"
""" % REPORT


def _project(tmp_path, monkeypatch, text=CONFIG):
    monkeypatch.setenv("PYTHON", sys.executable)
    repo = make_repo(tmp_path / "repo", {"a.cpp": "1\n2\n3\n4\n5\n", "tests.txt": "a b bad empty\n"})
    shutil.copy(FIXTURE / "fake_report.py", repo)
    (tmp_path / "build").mkdir()
    (tmp_path / "phase.toml").write_text(text)
    return config.load(tmp_path / "phase.toml", "p")


@pytest.fixture
def project(tmp_path, monkeypatch):
    return _project(tmp_path, monkeypatch)


def test_cover_maps_each_test_to_its_lines_and_keeps_the_baseline(project):
    assert covermap.cover(project, budget=600) == 0
    tests = st.load(project.state_path)["tests"]
    assert tests["a"]["lines"] == {"a.cpp": [1, 2]}
    assert tests["b"]["lines"] == {"a.cpp": [3]}
    assert tests["a"]["code_lines"] == {"a.cpp": [1, 2, 3, 4]}
    assert (tests["a"]["status"], tests["a"]["note"]) == ("passed", "")
    assert (tests["bad"]["status"], tests["bad"]["note"]) == ("failed", "exit 1")


def test_a_test_that_ran_nothing_is_a_failed_row(project):
    covermap.cover(project, budget=600)
    empty = st.load(project.state_path)["tests"]["empty"]
    assert (empty["status"], empty["note"]) == ("failed", "ran no test")


def test_a_failing_report_command_fails_the_row(tmp_path, monkeypatch):
    project = _project(tmp_path, monkeypatch, CONFIG.replace(REPORT, 'cover_cmds = [["false"]]'))
    covermap.cover(project, budget=600)
    a = st.load(project.state_path)["tests"]["a"]
    assert (a["status"], a["note"]) == ("failed", "report failed: false exited 1")


def test_cover_stops_on_its_budget_and_resumes(project):
    assert covermap.cover(project, budget=0) == 4
    assert covermap.cover(project, budget=600) == 0


def test_cover_reset_queues_every_test_again(project):
    covermap.cover(project, budget=600)
    assert covermap.cover(project, budget=0, reset=True) == 4


def test_cover_refuses_sources_changed_since_it_started(project):
    covermap.cover(project, budget=0)
    (project.repo / "a.cpp").write_text("changed\n")
    with pytest.raises(SystemExit, match="cover --reset"):
        covermap.cover(project, budget=600)


def test_cover_stops_when_the_unmutated_build_fails(tmp_path, monkeypatch):
    project = _project(tmp_path, monkeypatch, CONFIG.replace('build_cmd = ["true"]', 'build_cmd = ["false"]'))
    with pytest.raises(SystemExit, match="build failed"):
        covermap.cover(project, budget=600)


def test_readers():
    gcovr = {"files": [{"file": "a.cpp", "lines": [
        {"line_number": 1, "count": 2}, {"line_number": 2, "count": 0},
        {"line_number": 3, "count": 0, "gcovr/noncode": True}]}]}
    assert covermap.READERS["gcovr"](gcovr, "/r") == {os.path.realpath("/r/a.cpp"): ({1}, {1, 2})}
    coveragepy = {"files": {"/x/m.py": {"executed_lines": [1, 3], "missing_lines": [2]}}}
    assert covermap.READERS["coveragepy"](coveragepy, "/tmp") == {os.path.realpath("/x/m.py"): ({1, 3}, {1, 2, 3})}


def test_pruning_keeps_the_counters_of_target_objects_only(tmp_path):
    targets = (config.Target("src/plottables/multigraph-fill.cpp"),
               config.Target("src/datasource/binner.h", objects=("tests/auto/t/test-binner.cpp",)))
    names = covermap.counter_names(SimpleNamespace(targets=targets))
    assert names == {"multigraph-fill.cpp.gcda", "test-binner.cpp.gcda"}
    for name in ("src_plottables_multigraph-fill.cpp.gcda", "src_plottables_xmultigraph-fill.cpp.gcda",
                 "t_test-binner.cpp.gcda", "src_core.cpp.gcda"):
        (tmp_path / name).write_text("")
    covermap.prune_counters(tmp_path, names)
    assert sorted(p.name for p in tmp_path.iterdir()) == ["src_plottables_multigraph-fill.cpp.gcda", "t_test-binner.cpp.gcda"]

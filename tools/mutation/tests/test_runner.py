import shutil
import subprocess
import sys
from pathlib import Path

import pytest

import config
import covermap
import runner
import state as st
from fixture_repo import git, init_repo

FIXTURE = Path(__file__).parent / "fixture"

CONFIG = """
root = "."

[[project]]
name = "calc"
repo = "repo"
base = "main"
build_dir = "build"
build_cmd = ["bash", "-c", "mkdir -p {build_dir} && cd {build_dir} && g++ --coverage -O0 -c {repo}/calc.cpp -o calc.o && g++ --coverage -O0 -c {repo}/test_calc.cpp -o test_calc.o && g++ --coverage calc.o test_calc.o -o calc"]
test_cmd = ["{build_dir}/calc", "{test}"]
cover_test_cmd = ["{build_dir}/calc", "{test}"]
cover_cmds = [["gcovr", "--root", "{repo}", "--object-directory", "{build_dir}", "--json", "{scratch}/gcovr.json"]]
cover_reports = [{ kind = "gcovr", file = "{scratch}/gcovr.json", root = "{repo}" }]
failure_parser = "qtest"
tests = { regex_file = "repo/tests.txt", pattern = '(\\w+)' }

[[project.target]]
path = "calc.cpp"

[[project.equivalent]]
path = "calc.cpp"
op = "lt_to_le"
before = "    if (i < 0)"
reason = "i == 0 returns 0 either way"

[[project.equivalent]]
path = "calc.cpp"
op = "gt_to_ge"
before = "    if (i > 99)"
reason = "a line that no longer exists"

[[project]]
name = "py"
repo = "repo"
base = "main"
build_dir = "build-py"
build_cmd = ["true"]
test_env = { PYTHONPATH = "{build_dir}" }
test_cmd = ["{env_PYTHON}", "-c", "import limits; assert limits.clamp(-1, 0, 5) == 0; print('1 passed')"]
failure_parser = "pytest"

[[project.target]]
path = "limits.py"
deploy = "{build_dir}/limits.py"
"""


def _setup(tmp_path, name):
    repo = tmp_path / "repo"
    shutil.copytree(FIXTURE, repo, ignore=shutil.ignore_patterns("__pycache__"))
    init_repo(repo)
    (tmp_path / "phase.toml").write_text(CONFIG)
    return config.load(tmp_path / "phase.toml", name)


@pytest.fixture
def calc(tmp_path):
    project = _setup(tmp_path, "calc")
    assert covermap.cover(project, budget=600) == 0
    runner.plan(project)
    return project


def by_op(project):
    return {(m["line"], m["op"]): m for m in st.load(project.state_path)["mutants"].values()}


def test_plan_picks_the_covering_tests_per_line(calc):
    mutants = by_op(calc)
    assert mutants[(4, "int_plus_one")]["tests"] == ["low"]
    assert mutants[(6, "minus_to_plus")]["tests"] == ["high"]
    assert mutants[(3, "lt_to_le")]["status"] == "equivalent"


def test_plan_warns_about_equivalents_that_match_no_mutant(calc):
    stale = st.load(calc.state_path)["stale_equivalents"]
    assert [e["before"] for e in stale] == ["    if (i > 99)"]


def test_a_test_failing_its_baseline_runs_against_no_mutant(calc):
    data = st.load(calc.state_path)
    assert data["tests"]["broken"]["status"] == "failed"
    assert all("broken" not in m["tests"] for m in data["mutants"].values())


def test_run_kills_and_records_survivors(calc):
    assert runner.run(calc, budget=600) == 0
    mutants = by_op(calc)
    assert mutants[(6, "minus_to_plus")]["killed_by"] == ["Calc::high"]
    assert mutants[(4, "int_plus_one")]["killed_by"] == ["Calc::low"]
    assert set(mutants[(3, "negate_if")]["killed_by"]) == {"Calc::low", "Calc::high", "Calc::mid"}
    assert mutants[(5, "ge_to_gt")]["status"] == "survived"
    assert mutants[(3, "lt_to_le")]["killed_by"] == []


def test_a_capped_survivor_runs_every_covering_test_before_it_is_a_survivor(calc):
    calc.raw["max_tests"] = 1
    assert runner.run(calc, budget=600) == 0
    survivor = by_op(calc)[(5, "ge_to_gt")]
    assert survivor["status"] == "survived" and survivor["ran"] == survivor["covering"] == 2
    assert by_op(calc)[(6, "minus_to_plus")]["killed_by"] == ["Calc::high"]


def test_run_leaves_the_source_and_the_binary_unmutated(calc):
    original = st.read_source(FIXTURE / "calc.cpp")
    runner.run(calc, budget=600)
    assert st.read_source(calc.repo / "calc.cpp") == original
    for name in ("low", "high", "mid", "edge"):
        assert subprocess.run([str(calc.build_dir / "calc"), name], capture_output=True).returncode == 0, name


def test_run_stops_on_its_budget_and_resumes(calc):
    pending = sum(m["status"] == "pending" for m in st.load(calc.state_path)["mutants"].values())
    assert pending > 0
    assert runner.run(calc, budget=0) == pending
    assert runner.run(calc, budget=600) == 0


def test_a_mutant_that_does_not_compile_is_a_build_error(calc):
    data = st.load(calc.state_path)
    entry = {"path": "calc.cpp", "line": 3, "col": 4, "op": "manual", "before": "    if (i < 0)",
             "after": "    if (i <<<< 0)", "tests": ["low"], "ran": 0, "attempts": 0}
    assert runner.run_one(calc, data, entry)["status"] == "build_error"
    assert st.read_source(calc.repo / "calc.cpp") == st.read_source(FIXTURE / "calc.cpp")


def test_run_first_restores_what_a_killed_run_left(calc):
    path = calc.repo / "calc.cpp"
    backup = calc.run_dir / "backup" / "calc.cpp.orig"
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, backup)
    data = st.load(calc.state_path)
    data["in_flight"] = [{"path": str(path), "backup": str(backup), "sha": st.sha(path)}]
    st.save(calc.state_path, data)
    st.write_source(path, "garbage\n")
    runner.run(calc, budget=0)
    assert st.read_source(path) == st.read_source(FIXTURE / "calc.cpp")


def test_run_refuses_sources_changed_since_the_plan(calc):
    path = calc.repo / "calc.cpp"
    st.write_source(path, st.read_source(path).replace("return i;", "return i + 0;"))
    with pytest.raises(SystemExit, match="cover --reset"):
        runner.run(calc, budget=600)


def test_plan_refuses_a_target_with_uncommitted_changes(calc):
    path = calc.repo / "calc.cpp"
    st.write_source(path, st.read_source(path) + "// note\n")
    with pytest.raises(SystemExit, match="uncommitted changes"):
        runner.plan(calc)


def test_after_a_fix_cover_reset_and_plan_remake_the_file(calc):
    runner.run(calc, budget=600)
    path = calc.repo / "calc.cpp"
    st.write_source(path, st.read_source(path).replace("    return i;\n", "    return i; // clamped\n"))
    git(calc.repo, "commit", "-qam", "fix")
    with pytest.raises(SystemExit, match="cover --reset"):
        runner.plan(calc)
    covermap.cover(calc, budget=600, reset=True)
    data = runner.plan(calc)
    assert {m["status"] for m in data["mutants"].values()} == {"pending", "equivalent"}


def test_plan_requeues_a_survivor_a_new_test_covers(calc):
    runner.run(calc, budget=600)
    assert by_op(calc)[(5, "ge_to_gt")]["status"] == "survived"
    tests_txt = calc.repo / "tests.txt"
    tests_txt.write_text(tests_txt.read_text().strip() + " edge\n")
    covermap.cover(calc, budget=600, reset=True)
    runner.plan(calc)
    assert by_op(calc)[(5, "ge_to_gt")]["status"] == "pending"
    runner.run(calc, budget=600)
    assert by_op(calc)[(5, "ge_to_gt")]["killed_by"] == ["Calc::edge"]


def test_try_runs_one_mutant_of_the_current_source(calc):
    result = runner.try_mutant(calc, "calc.cpp", 5, "ge_to_gt", ["edge"])
    assert result["status"] == "killed" and result["killed_by"] == ["Calc::edge"]
    assert st.read_source(calc.repo / "calc.cpp") == st.read_source(FIXTURE / "calc.cpp")


def test_a_python_mutant_goes_to_the_deploy_copy_only(tmp_path, monkeypatch):
    monkeypatch.setenv("PYTHON", sys.executable)
    project = _setup(tmp_path, "py")
    project.build_dir.mkdir(parents=True)
    shutil.copyfile(project.repo / "limits.py", project.build_dir / "limits.py")
    data = st.load(project.state_path)
    data["tests"] = {"t": {"status": "passed", "seconds": 0.1,
                           "lines": {"limits.py": [1, 2, 3, 4]}, "code_lines": {"limits.py": [1, 2, 3, 4]}}}
    data["cover_files"] = covermap.source_shas(project)
    st.save(project.state_path, data)
    source = project.repo / "limits.py"
    stamp = source.stat().st_mtime_ns
    runner.plan(project)
    assert runner.run(project, budget=600) == 0
    assert source.stat().st_mtime_ns == stamp
    assert st.read_source(project.build_dir / "limits.py") == st.read_source(source)
    mutants = by_op(project)
    assert mutants[(2, "lt_to_le")]["status"] == "survived"
    assert mutants[(2, "negate_if")]["killed_by"] == ["t::<exit 1>"]


def test_a_line_with_an_empty_covering_list_is_uncovered_and_never_planned(calc, monkeypatch):
    data = st.load(calc.state_path)
    data["mutants"] = {}
    st.save(calc.state_path, data)
    real = runner.covering_tests
    monkeypatch.setattr(runner, "covering_tests", lambda tests: {**real(tests), "calc.cpp": {**real(tests)["calc.cpp"], 3: []}})
    data = runner.plan(calc)
    assert all(m["line"] != 3 for m in data["mutants"].values() if m["status"] == "pending")
    assert 3 in data["uncovered"]["calc.cpp"]
    assert all(m["tests"] for m in data["mutants"].values())


def test_max_tests_zero_means_all_and_terminates(calc):
    calc.raw["max_tests"] = 0
    assert runner.run(calc, budget=600) == 0
    assert by_op(calc)[(5, "ge_to_gt")]["status"] == "survived"


def test_try_refuses_an_empty_test_list(calc):
    with pytest.raises(SystemExit, match="no tests"):
        runner.try_mutant(calc, "calc.cpp", 5, "ge_to_gt", [])


def test_try_refuses_tests_that_fail_without_the_mutant(calc):
    calc.raw["test_cmd"] = ["false"]
    with pytest.raises(SystemExit, match="fail without the mutant"):
        runner.try_mutant(calc, "calc.cpp", 5, "ge_to_gt", ["edge"])
    assert st.read_source(calc.repo / "calc.cpp") == st.read_source(FIXTURE / "calc.cpp")


def test_a_test_that_ran_nothing_is_fatal_not_a_kill(calc):
    calc.raw["test_cmd"] = ["true"]
    with pytest.raises(SystemExit, match="ran no test: run cover --reset, then plan"):
        runner.run(calc, budget=600)
    assert st.read_source(calc.repo / "calc.cpp") == st.read_source(FIXTURE / "calc.cpp")
    assert all(m["status"] != "killed" for m in st.load(calc.state_path)["mutants"].values())


def test_run_tests_with_no_names_runs_nothing(calc):
    assert runner._run_tests(calc, [], {}) == ([], 0.0)


def test_a_target_without_any_coverage_is_listed_as_not_measured(calc):
    data = st.load(calc.state_path)
    for entry in data["tests"].values():
        entry["code_lines"], entry["lines"] = {}, {}
    st.save(calc.state_path, data)
    assert runner.plan(calc)["not_measured"] == ["calc.cpp"]


def test_run_before_plan_names_plan(tmp_path):
    project = _setup(tmp_path, "calc")
    with pytest.raises(SystemExit, match="plan"):
        runner.run(project, budget=600)


def test_a_stale_deploy_copy_stops_every_build(tmp_path, monkeypatch):
    monkeypatch.setenv("PYTHON", sys.executable)
    project = _setup(tmp_path, "py")
    project.build_dir.mkdir(parents=True)
    (project.build_dir / "limits.py").write_text("old = 1\n")
    with pytest.raises(SystemExit, match="deployed copy is stale: rebuild .*true"):
        covermap.cover(project, budget=600)
    with pytest.raises(SystemExit, match="deployed copy is stale"):
        covermap.build_or_die(project)

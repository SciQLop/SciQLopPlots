import sys

import pytest

import config
from fixture_repo import git, make_repo

PY = "def keep(x):\n    if x > 1:\n        return x\n    return 0\n\n\ndef drop(x):\n    return x + 1\n"

CONFIG = """
root = "."

[[project]]
name = "p"
repo = "repo"
base = "main"
build_dir = "build"
env = { GREETING = "hi {test}" }
test_cmd = ["run", "{tests}", "--in", "{build_dir}"]
tests = { glob = ["repo/test_*.py"], prefix = "{root}/" }

[[project.target]]
path = "a.cpp"
scope = "diff"

[[project.target]]
path = "m.py"
functions = ["keep"]
deploy = "{build_dir}/m.py"

[[project.equivalent]]
path = "a.cpp"
op = "eq_to_ne"
before = "TWO"
reason = "demo"

[[project]]
name = "q"
repo = "repo"
base = "main"
build_dir = "build-q"
tests = { regex_file = "repo/autotest.cpp", pattern = 'QCPTEST\\((\\w+)\\);' }

[[project.target]]
path = "a.cpp"

[[project]]
name = "r"
repo = "repo"
base = "main"
build_dir = "build-r"
test_env = { CLS = "{test_class}", FN = "{test_function}" }
tests = { regex_file = "repo/autotest.cpp", pattern = 'QCPTEST\\((\\w+)\\);', functions_cmd = ["{env_PYTHON}", "-c", "import os; print(os.environ['CLS'].lower() + '_one()'); print('QWARN : noise'); print('two()' if os.environ.get('LIST') == '1' else '')"], functions_env = { LIST = "1" }, functions_pattern = '^(\\w+)\\(\\)$' }

[[project.target]]
path = "a.cpp"
"""


@pytest.fixture
def root(tmp_path):
    repo = make_repo(tmp_path / "repo", {
        "a.cpp": "one\ntwo\nthree\nfour\n",
        "m.py": PY,
        "test_a.py": "",
        "test_b.py": "",
        "autotest.cpp": "#define QCPTEST(t) x\nQCPTEST(TestA);\nQCPTEST(TestB);\n",
    })
    git(repo, "checkout", "-qb", "feature")
    (repo / "a.cpp").write_text("one\nTWO\nthree\nfour\nfive\n")
    git(repo, "commit", "-qam", "change")
    (tmp_path / "phase.toml").write_text(CONFIG)
    return tmp_path


def test_load_resolves_paths_and_deploy(root):
    project = config.load(root / "phase.toml", "p")
    assert project.repo == (root / "repo").resolve()
    assert project.run_dir == (root / "build" / "mutation").resolve()
    assert project.targets[1].deploy == str((root / "build" / "m.py").resolve())
    assert project.deploy_path(project.targets[0]) == project.repo / "a.cpp"
    assert project.equivalents[0].op == "eq_to_ne"


def test_unknown_project_is_a_clear_error(root):
    with pytest.raises(SystemExit, match="no project 'nope'"):
        config.load(root / "phase.toml", "nope")


def test_tests_expand_to_one_argument_each(root):
    project = config.load(root / "phase.toml", "p")
    base = str(project.root)
    assert project.command("test_cmd", ["repo/test_a.py", "repo/test_b.py"]) == [
        "run", f"{base}/repo/test_a.py", f"{base}/repo/test_b.py", "--in", str(project.build_dir)]


def test_env_templates_and_gcov_prefix(root):
    project = config.load(root / "phase.toml", "p")
    assert project.env(["x"])["GREETING"] == "hi x"
    assert "GCOV_PREFIX" not in project.env()
    assert project.env(mutation=True)["GCOV_PREFIX"] == str(project.run_dir / "gcda-junk")


def test_diff_scope_is_the_changed_lines(root):
    project = config.load(root / "phase.toml", "p")
    assert config.target_lines(project, project.targets[0]) == {2, 5}


def test_python_functions_limit_the_lines(root):
    project = config.load(root / "phase.toml", "p")
    assert config.target_lines(project, project.targets[1]) == {1, 2, 3, 4}


def test_discover_tests_by_glob_and_by_regex(root):
    assert config.discover_tests(config.load(root / "phase.toml", "p")) == ["repo/test_a.py", "repo/test_b.py"]
    assert config.discover_tests(config.load(root / "phase.toml", "q")) == ["TestA", "TestB"]


def test_functions_are_listed_per_class_and_split_back_into_class_and_function(root, monkeypatch):
    monkeypatch.setenv("PYTHON", sys.executable)
    project = config.load(root / "phase.toml", "r")
    assert config.discover_tests(project) == ["TestA::testa_one", "TestA::two", "TestB::testb_one", "TestB::two"]
    env = project.env(["TestA::two"])
    assert (env["CLS"], env["FN"]) == ("TestA", "two")

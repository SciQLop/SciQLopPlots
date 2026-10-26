import dataclasses
from pathlib import Path

import pytest

import config
import covermap

CONFIG = Path(__file__).resolve().parents[1] / "phase1.toml"


@pytest.mark.parametrize("name", ["neoqcp", "sciqlopplots"])
def test_every_target_exists_and_has_lines(name):
    project = config.load(CONFIG, name)
    for target in project.targets:
        assert (project.repo / target.path).is_file(), target.path
        assert config.target_lines(project, target), target.path
        for source in target.objects:
            assert (project.repo / source).is_file(), source


def test_neoqcp_runs_one_qtest_function_at_a_time():
    project = config.load(CONFIG, "neoqcp")
    classes = config.discover_classes(project)
    assert {"TestMultiGraph", "TestFillGeometry", "TestHistogramPlottable", "TestErrorBarGeometry", "TestPipeline"} <= set(classes)
    assert len(classes) == len(set(classes)) > 40
    assert project.raw["tests"]["functions_env"] == {"QCP_TEST_FUNCTION": "-functions"}
    env = project.env(["TestFillGeometry::twoRowsMakeOneQuad"])
    assert (env["QCP_TEST_CLASS"], env["QCP_TEST_FUNCTION"]) == ("TestFillGeometry", "twoRowsMakeOneQuad")


def test_neoqcp_keeps_the_counters_of_its_targets_and_the_binner_includers():
    names = covermap.counter_names(config.load(CONFIG, "neoqcp"))
    assert {"multigraph-fill.cpp.gcda", "plottable-histogram.cpp.gcda", "test-histogram1d.cpp.gcda"} <= names
    assert not any(n.endswith(".h.gcda") for n in names)


def test_sciqlopplots_discovers_the_pytest_files_as_absolute_arguments():
    project = config.load(CONFIG, "sciqlopplots")
    names = config.discover_tests(project)
    assert "tests/integration/test_fills_errorbars.py" in names and len(names) > 150
    assert project.command("test_cmd", names[:1])[3] == str(project.root / names[0])


def test_the_python_target_is_the_band_error_bar_and_histogram_helpers():
    project = config.load(CONFIG, "sciqlopplots")
    target = next(t for t in project.targets if t.path.endswith(".py"))
    assert target.deploy == str(project.build_dir / "SciQLopPlots" / "__init__.py")
    assert 75 < len(config.target_lines(project, target)) < 200


def test_every_listed_python_function_contributes_lines():
    project = config.load(CONFIG, "sciqlopplots")
    target = next(t for t in project.targets if t.path.endswith(".py"))
    for name in target.functions:
        single = dataclasses.replace(target, functions=(name,))
        assert config.target_lines(project, single), name


def test_coveragepy_reports_resolve_against_the_repo_root_and_only_no_data_falls_back():
    project = config.load(CONFIG, "sciqlopplots")
    spec = next(s for s in project.raw["cover_reports"] if s["kind"] == "coveragepy")
    assert spec["root"] == "{root}"
    command = next(c for c in project.raw["cover_cmds"] if c[0] == "sh")[-1]
    assert "No data to report" in command and "exit 1" in command

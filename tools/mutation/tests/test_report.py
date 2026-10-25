import json
from types import SimpleNamespace

import report


def mutant(line, op, status, tests=("TestA",), killed_by=(), before="    if (a || b)", after="    if (a && b)", path="a.cpp"):
    return {"path": path, "line": line, "col": 4, "op": op, "before": before, "after": after, "status": status,
            "tests": list(tests), "covering": len(tests), "ran": len(tests), "killed_by": list(killed_by),
            "seconds": 1.0, "build_seconds": 0.5, "attempts": 1}


DATA = {
    "tests": {"TestA": {"status": "passed", "note": ""}, "TestX": {"status": "failed", "note": "ran no test"}},
    "mutants": {
        "a.cpp:3:4:or_to_and": mutant(3, "or_to_and", "survived"),
        "a.cpp:5:4:lt_to_le": mutant(5, "lt_to_le", "killed", ("TestA", "TestB"), ("TestA::f", "TestB::g")),
        "a.cpp:7:4:negate_if": mutant(7, "negate_if", "killed", ("TestA",), ("TestA::f",)),
        "a.cpp:9:4:eq_to_ne": mutant(9, "eq_to_ne", "killed", ("TestC",), ("TestC::h",)),
        "b.cpp:2:4:true_to_false": mutant(2, "true_to_false", "build_error", path="b.cpp"),
        "b.cpp:4:4:plus_to_minus": mutant(4, "plus_to_minus", "survived", ("TestD",), path="b.cpp"),
    },
    "uncovered": {"a.cpp": [10, 11, 12, 20], "b.cpp": []},
    "not_measured": ["c.cpp"],
    "stale_equivalents": [{"path": "a.cpp", "op": "eq_to_ne", "before": "    x == y", "reason": "demo"}],
}


def test_ranges():
    assert report.ranges([12, 10, 11, 20, 22, 21]) == "10-12, 20-22"
    assert report.ranges([5]) == "5"
    assert report.ranges([]) == ""


def test_survivor_rows_escape_pipes_and_leave_triage_empty():
    text = report.render("p", DATA)
    assert "| a.cpp:3:4:or_to_and | a.cpp:3 | or_to_and | `if (a \\|\\| b)` | `if (a && b)` | 1/1 |  |" in text
    assert "a.cpp:5:4:lt_to_le |" not in text


def test_counts_and_score():
    text = report.render("p", DATA)
    assert "| survived | 2 |" in text and "| killed | 3 |" in text and "| build_error | 1 |" in text
    assert "Mutation score (killed + timeout over decided): 60.0%" in text


def test_uncovered_lines_baseline_notes_and_stale_equivalents():
    text = report.render("p", DATA)
    assert "- a.cpp: 10-12, 20" in text
    assert "- b.cpp:" not in text
    assert "- TestX: ran no test" in text
    assert "- a.cpp eq_to_ne `x == y`: demo" in text


def test_kill_matrix_and_redundancy():
    matrix = report.kill_matrix(DATA)
    assert matrix["TestA::f"] == ["a.cpp:5:4:lt_to_le", "a.cpp:7:4:negate_if"]
    assert report.redundancy(DATA) == (["TestD"], ["TestB::g"])


def test_fail_fast_runs_say_killed_by_is_the_fastest_killer_only():
    assert "fastest killing test only" in report.render("p", DATA, fail_fast=True)
    assert "fastest killing test only" not in report.render("p", DATA)


def test_write_creates_the_report_and_the_matrix(tmp_path):
    project = SimpleNamespace(name="p", run_dir=tmp_path / "run", raw={})
    out = report.write(project, DATA, tmp_path / "r.md")
    assert out == tmp_path / "r.md" and out.read_text().startswith("# Mutation report: p")
    assert json.loads((tmp_path / "r-kill-matrix.json").read_text())["TestC::h"] == ["a.cpp:9:4:eq_to_ne"]
    assert report.write(project, DATA) == tmp_path / "run" / "report.md"


def test_not_measured_targets_are_listed():
    text = report.render("p", DATA)
    assert "## Targets without coverage" in text and "- c.cpp" in text


def _per_function(killed_by):
    tests = ["TestA::f", "TestA::g"]
    data = {"tests": {}, "mutants": {"m": mutant(1, "x", "killed", tests, killed_by)}}
    return report.redundancy(data)


def test_redundancy_credits_only_the_per_function_killer():
    assert _per_function(["TestA::g"]) == (["TestA::f"], [])


def test_redundancy_credits_every_function_of_a_class_for_a_bare_class_killer():
    assert _per_function(["TestA"]) == ([], [])


def test_cell_replaces_backticks_so_the_span_cannot_end_early():
    assert report.cell("a `b` | c") == "`a 'b' \\| c`"

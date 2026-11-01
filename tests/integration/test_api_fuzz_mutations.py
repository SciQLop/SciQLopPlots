"""Bad-argument cases: one argument replaced at a time, plus wrong argument counts and deleted objects."""
import re

from tests.sweeps.api_fuzz.mutations import BAD_VALUES, cases_for


def row(*arg_kinds, kind="plot", name="set_thing"):
    return {"kind": kind, "cls": "SciQLopPlot", "name": name, "found": {"index": 0, "arg_kinds": list(arg_kinds)}}


def replaced(case):
    return [int(i) for i in re.findall(r"^args\[(\d+)\] = ", case.call, re.M)]


def test_one_argument_mutated_at_a_time():
    cases = [c for c in cases_for(row("number", "buffer"), receiver_deletable=False) if replaced(c)]
    assert cases and all(len(replaced(c)) == 1 for c in cases)
    assert {replaced(c)[0] for c in cases} == {0, 1}


def test_buffer_parameters_get_buffer_values():
    buffer_values = {b.name for b in BAD_VALUES if "buffer" in b.applies_to}
    cases = cases_for(row("buffer"), receiver_deletable=False)
    assert buffer_values and all(any(f":{v}@0" in c.id for c in cases) for v in buffer_values)


def test_mismatched_lengths_expect_value_error():
    cases = cases_for(row("buffer", "buffer"), receiver_deletable=False)
    assert [c.expect for c in cases if ":shorter@" in c.id] == ["value"]


def test_deleted_argument_cases_generated():
    cases = cases_for(row("deletable"), receiver_deletable=False)
    assert [c.expect for c in cases if ":deleted@0" in c.id] == ["deleted"]


def test_deleted_receiver_only_when_deletable():
    assert any(c.id.endswith(":deleted_receiver") for c in cases_for(row(), receiver_deletable=True))
    assert not any(c.id.endswith(":deleted_receiver") for c in cases_for(row(), receiver_deletable=False))


def test_count_cases():
    ids = [c.id for c in cases_for(row("number"), receiver_deletable=False)]
    assert any(i.endswith(":too_few") for i in ids) and any(i.endswith(":too_many") for i in ids)


def test_baseline_case_first():
    first = cases_for(row("number"), receiver_deletable=False)[0]
    assert first.expect == "ok" and first.id.endswith(":baseline")

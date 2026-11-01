"""The API fuzz harness: cases run out of process, and each outcome is graded against the contract."""
import pytest

from tests.sweeps.api_fuzz.harness import Case, classify, run_batch


def grade(*cases, **kwargs):
    return [classify(case, outcome).status for case, outcome in zip(cases, run_batch(list(cases), **kwargs))]


def case(call, expect="ok", setup="", id=None):
    return Case(id=id or call[:40], setup=setup, call=call, expect=expect)


@pytest.mark.parametrize("call, expect, status", [
    ("import os; os.abort()", "ok", "crash"),
    ("import sys; sys.stderr.write('boom\\n')", "ok", "swallowed"),
    ("raise TypeError('x must be an array')", "type", "pass"),
    ("raise TypeError('')", "type", "bad_message"),
    ("raise TypeError('Error')", "type", "bad_message"),
    ("raise TypeError('failed')", "type", "bad_message"),
    ("raise ValueError('bad x')", "type", "wrong_class"),
    ("raise KeyError('k')", "value", "unexpected_class"),
    ("pass", "value", "no_error"),
    ("pass", "ok_or_value", "pass"),
    ("raise OverflowError('x is too large')", "ok_or_value", "pass"),
    ("raise ValueError('x is negative')", "ok", "baseline_broken"),
    ("raise RuntimeError('Internal C++ object (SciQLopPlot) already deleted.')", "deleted", "pass"),
    ("import sys; sys.stderr.write('QRhiWidget: QRhi is not supported on this platform.\\n')", "ok", "pass"),
    # A call that raised the right class is graded on its exception, not on what it also printed.
    ("import sys; sys.stderr.write('warning\\n'); raise TypeError('x must be an array')", "type", "pass"),
])
def test_status(call, expect, status):
    assert grade(case(call, expect)) == [status]


def test_hang():
    assert grade(case("import time; time.sleep(60)"), per_case_s=2) == ["hang"]


def test_crash_in_batch_keeps_neighbours():
    assert grade(case("pass", id="a"), case("import os; os.abort()", id="b"), case("pass", id="c")) == \
        ["pass", "crash", "pass"]


def test_setup_output_is_not_the_calls():
    assert grade(case("pass", setup="import sys; sys.stderr.write('setup noise\\n')")) == ["pass"]


def test_queued_errors_belong_to_the_call():
    queued = "from PySide6.QtCore import QTimer; import sys; QTimer.singleShot(0, lambda: sys.stderr.write('late\\n'))"
    assert grade(case(queued)) == ["swallowed"]


def test_broken_setup_is_a_broken_baseline():
    assert grade(case("pass", setup="raise ValueError('no receiver')")) == ["baseline_broken"]

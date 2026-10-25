import outcomes
from proc import Result

QTEST = ("PASS   : TestA::initTestCase()\nPASS   : TestA::one()\nFAIL!  : TestA::two(row) Compared values\n"
         "FAIL!  : TestA::two(other) x\nPASS   : TestA::cleanupTestCase()\n")


def r(code, output):
    return Result(code, output, 1.0)


def test_failures_by_parser():
    pytest_out = "FAILED tests/integration/test_x.py::test_y[1] - AssertionError\nERROR tests/unit/test_z.py::test_w\n1 failed, 1 error in 0.1s\n"
    assert outcomes.failures("qtest", r(1, QTEST), ["TestA::two"]) == ["TestA::two"]
    assert outcomes.failures("pytest", r(1, pytest_out), ["f"]) == [
        "tests/integration/test_x.py::test_y[1]", "tests/unit/test_z.py::test_w"]
    assert outcomes.failures("qtest", r(-11, "Received signal"), ["TestA::one"]) == ["TestA::one::<exit -11>"]
    assert outcomes.failures("qtest", r(None, ""), ["TestA::one"]) == ["TestA::one::<timeout>"]
    assert outcomes.failures("qtest", r(0, "PASS   : TestA::one()\n"), ["TestA::one"]) == []


def test_a_clean_exit_that_ran_nothing_is_a_failure():
    only_fixtures = "PASS   : TestA::initTestCase()\nPASS   : TestA::cleanupTestCase()\nTotals: 2 passed\n"
    assert outcomes.failures("qtest", r(0, ""), ["TestA::one"]) == ["TestA::one::<ran no test>"]
    assert outcomes.failures("qtest", r(0, only_fixtures), ["TestA::one"]) == ["TestA::one::<ran no test>"]
    assert outcomes.failures("pytest", r(0, "no tests ran in 0.01s\n"), ["f.py"]) == ["f.py::<ran no test>"]


def test_executed_counts():
    assert outcomes.executed("qtest", QTEST) == 3
    assert outcomes.executed("pytest", "1 failed, 2 passed, 1 error in 3.0s") == 4
    assert outcomes.executed("pytest", "collected 0 items\nno tests ran") == 0

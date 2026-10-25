"""Reading a test run: which tests failed, and whether any test ran at all.

A run that exits 0 but executed nothing (a renamed class, a typo, every test
skipped) is a failure, never a pass: otherwise every mutant it guards survives.
"""
import re

FAILED = {
    "qtest": re.compile(r"^FAIL!\s+:\s+(\w+::\w+)\(", re.M),
    "pytest": re.compile(r"^(?:FAILED|ERROR) (\S+)", re.M),
}
_QTEST_RAN = re.compile(r"^(?:PASS|FAIL!)\s+:\s+\w+::(?!initTestCase\(|cleanupTestCase\()\w+\(", re.M)
_PYTEST_RAN = re.compile(r"\b(\d+) (?:passed|failed|errors?)\b")
EXECUTED = {
    "qtest": lambda output: len(_QTEST_RAN.findall(output)),
    "pytest": lambda output: sum(int(n) for n in _PYTEST_RAN.findall(output)),
}


def executed(parser, output):
    return EXECUTED[parser](output)


def failures(parser, result, batch):
    if result.timed_out:
        return [f"{n}::<timeout>" for n in batch]
    found = list(dict.fromkeys(FAILED[parser].findall(result.output)))
    if result.code != 0:
        return found or [f"{n}::<exit {result.code}>" for n in batch]
    return [] if executed(parser, result.output) else [f"{n}::<ran no test>" for n in batch]

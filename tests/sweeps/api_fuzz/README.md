# API fuzz sweep

The contract (`docs/plans/2026-11-01-api-fuzzing-design.md`): every public call works, or raises
`TypeError` (wrong type), `ValueError` (bad value) or `RuntimeError` (deleted object) with a message
that says why. It never crashes, hangs, or prints an error and returns as if nothing happened.

`harness.py` runs cases (setup source + call source + what is expected) in batches, one child
interpreter per batch. A case that crashes or hangs is named by the stderr sentinels, and the cases
after it run again in a fresh child. Stderr printed during the call and the 200 ms of event processing
after it belongs to the case; lines in `STDERR_ALLOW` are known-harmless platform noise.

`catalogue.py` lists every public method (shiboken signatures, plus the Python wrappers from
`SciQLopPlots/__init__.py`), maps each class to the receiver kinds that are instances of it (`child.KINDS`,
built on `tests/sweeps/self_delete/common.py`), and finds a working baseline call for each method by
trying argument guesses in the child. `mutations.py` turns a baseline into bad-argument cases: one
argument replaced at a time, wrong argument counts, deleted objects.

```
cd /tmp   # from the repo root the source package would shadow the build
P=~/Documents/prog/SciQLopPlots
QT_QPA_PLATFORM=offscreen PYTHONPATH=$P/build-venv:$P $P/.venv/bin/python -m tests.sweeps.api_fuzz.driver sweep [--classes A,B] [--jobs 4]
QT_QPA_PLATFORM=offscreen PYTHONPATH=$P/build-venv:$P $P/.venv/bin/python -m tests.sweeps.api_fuzz.driver folder DIR
```

Reports: `$SCIQLOP_PLOTS_TEST_REPORTS/api_fuzz.jsonl` (every verdict, with the call source) and
`api_fuzz_summary.md` (counts, failures grouped by method). In pytest, `tests/integration/test_api_contract.py`
sweeps the classes in `CI_CLASSES`; `SCIQLOP_PLOTS_DEEP_FUZZ=1` adds the whole API.

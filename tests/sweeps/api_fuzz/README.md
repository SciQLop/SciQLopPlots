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
argument replaced at a time, wrong argument counts, deleted objects, and the baseline from a plain Python
thread (a setter queues to the GUI thread, a factory raises RuntimeError; `USER_IMPLEMENTED` pure
virtuals are skipped).

```
cd /tmp   # from the repo root the source package would shadow the build
P=~/Documents/prog/SciQLopPlots
QT_QPA_PLATFORM=offscreen PYTHONPATH=$P/build-venv:$P $P/.venv/bin/python -m tests.sweeps.api_fuzz.driver sweep [--classes A,B] [--jobs 4]
QT_QPA_PLATFORM=offscreen PYTHONPATH=$P/build-venv:$P $P/.venv/bin/python -m tests.sweeps.api_fuzz.driver folder DIR
```

Reports: `$SCIQLOP_PLOTS_TEST_REPORTS/api_fuzz.jsonl` (every verdict, with the call source) and
`api_fuzz_summary.md` (counts, failures grouped by method). In pytest, `tests/integration/test_api_contract.py`
sweeps the classes in `CI_CLASSES`; `SCIQLOP_PLOTS_DEEP_FUZZ=1` adds the whole API.

The whole API is about 12000 cases and 40 minutes: pass a few classes per call with `--classes` to keep
each run short. The GPU paths need a display:
`xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb ... driver sweep --classes SciQLopLineGraph`
(keep `timeout` inside `xvfb-run`, so a killed run takes its Xvfb with it).

The story fuzzer (`tests/fuzzing`) chains actions on a live panel. `SCIQLOP_PLOTS_DEEP_FUZZ=1` adds a
deep run (100 stories of 50 steps, in a child process); its full log goes to
`$SCIQLOP_PLOTS_TEST_REPORTS/deep_story_fuzz.log`. `test_story_coverage.py` writes the public methods no
story calls to `story_coverage.md`.

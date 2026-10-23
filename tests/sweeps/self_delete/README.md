# Self-delete sweep

For every setter of every plottable, item and component: connect each signal it fires to a slot
that deletes the object (or, for a component, its graph), call the setter, and record whether the
process survives. One subprocess per pair, sequential, poisoned malloc. Not collected by pytest.

```
cd tests/sweeps/self_delete
../../../.venv/bin/python driver.py discover   # finds working args and the signals each setter fires
../../../.venv/bin/python driver.py pairs      # runs every (setter, signal) pair, ~15 min
../../../.venv/bin/python driver.py summary    # counts, then each CRASH / HANG pair
./bt.sh line_graph set_busy 0 self busy_changed self   # gdb backtrace of one pair
```

`SQP_BUILD` picks the build dir (default `build-venv`), `SWEEP_OUT` where the jsonl files go.
Delete `discovery.jsonl` / `pairs.jsonl` for a fresh run: both phases resume from them.

# Self-delete sweep

For every setter of every plottable, item and component: connect each signal it fires to a slot
that deletes the object (or, for a component, its graph), call the setter, and record whether the
process survives. One subprocess per pair, sequential, poisoned malloc. Not collected by pytest.

```
cd tests/sweeps/self_delete
../../../.venv/bin/python driver.py discover   # finds working args and the signals each setter fires
../../../.venv/bin/python driver.py pairs      # runs every (setter, signal) pair, ~15 min
../../../.venv/bin/python driver.py summary    # setter and pair counts per status, then the odd ones
./bt.sh line_graph set_busy 0 self busy_changed self   # gdb backtrace of one pair
```

`SQP_BUILD` picks the build dir (default `build-venv`), `SWEEP_OUT` where the jsonl files go,
`QT_QPA_PLATFORM` the platform (default `offscreen`, which never paints through QRhi). GPU paths:
`xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb ../../../.venv/bin/python driver.py pairs`.
Delete `discovery.jsonl` / `pairs.jsonl` for a fresh run: both phases resume from them.

Setter statuses: `emitted` (pairs run), `no emit` (ran, fired nothing), `no args` (every candidate
argument raised: the setter never ran, add arguments in `common.candidates` or state in
`common.PREPARE`), `unsupported` (the class refuses it), `crashed` (discovery itself died).

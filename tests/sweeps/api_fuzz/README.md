# API fuzz sweep

The contract (`docs/plans/2026-11-01-api-fuzzing-design.md`): every public call works, or raises
`TypeError` (wrong type), `ValueError` (bad value) or `RuntimeError` (deleted object) with a message
that says why. It never crashes, hangs, or prints an error and returns as if nothing happened.

`harness.py` runs cases (setup source + call source + what is expected) in batches, one child
interpreter per batch. A case that crashes or hangs is named by the stderr sentinels, and the cases
after it run again in a fresh child. Stderr printed during the call and the 200 ms of event processing
after it belongs to the case; lines in `STDERR_ALLOW` are known-harmless platform noise.

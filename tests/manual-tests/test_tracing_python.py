"""Verify Python-side access to the C++ runtime tracer."""
import json
import os
import sys
import tempfile

from SciQLopPlots import tracing


def _read_trace(path):
    with open(path) as f:
        return json.load(f)


def test_basic_zone():
    fd, path = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    try:
        tracing.enable(path)
        with tracing.zone("hello", cat="py"):
            pass
        tracing.disable()

        events = _read_trace(path)["traceEvents"]
        x = [e for e in events if e["ph"] == "X"]
        assert len(x) == 1, x
        assert x[0]["name"] == "hello"
        assert x[0]["cat"] == "py"
    finally:
        os.unlink(path)


def test_zone_with_args():
    fd, path = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    try:
        tracing.enable(path)
        with tracing.zone("fetch", cat="data",
                          n_points=12345, ratio=0.5,
                          ok=True, product="amda/mms_fgm"):
            pass
        tracing.disable()

        events = _read_trace(path)["traceEvents"]
        x = next(e for e in events if e["name"] == "fetch")
        assert x["args"]["n_points"] == 12345
        assert x["args"]["ratio"] == 0.5
        assert x["args"]["ok"] is True
        assert x["args"]["product"] == "amda/mms_fgm"
    finally:
        os.unlink(path)


def test_counter_and_async():
    fd, path = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    try:
        tracing.enable(path)
        tracing.counter("queue_depth", 7.0, cat="fetch")
        h = tracing.async_begin("download", cat="data")
        tracing.async_end(h)
        tracing.disable()

        events = _read_trace(path)["traceEvents"]
        phases = sorted({e["ph"] for e in events})
        assert "C" in phases
        assert "b" in phases
        assert "e" in phases
    finally:
        os.unlink(path)


def test_traced_decorator():
    fd, path = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    try:
        tracing.enable(path)

        @tracing.traced("annotated", cat="py")
        def work(x):
            return x * 2

        assert work(3) == 6
        tracing.disable()

        events = _read_trace(path)["traceEvents"]
        x = [e for e in events if e["ph"] == "X" and e["name"] == "annotated"]
        assert len(x) == 1
    finally:
        os.unlink(path)


def main():
    tests = [test_basic_zone, test_zone_with_args, test_counter_and_async, test_traced_decorator]
    failed = []
    for t in tests:
        try:
            t()
            print(f"PASS {t.__name__}")
        except Exception as e:
            print(f"FAIL {t.__name__}: {e!r}")
            failed.append(t.__name__)
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()

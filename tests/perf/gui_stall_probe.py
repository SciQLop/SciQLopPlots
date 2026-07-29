#!/usr/bin/env python3
"""Instruments for attributing GUI stalls, and a self-test that validates them.

Two probes; the *pair* is what discriminates. Either alone misleads.

  Heartbeat  a QTimer on the GUI thread. Its lateness IS the freeze the user sees.
             Also samples the GUI thread's CPU time per tick, so a stall can be
             classified as compute-bound or blocked.
  GILProbe   a plain Python thread sleeping in short slices. time.sleep releases
             the GIL, so an over-long gap proves a *Python* thread held it. On
             regaining the GIL it snapshots every stack, catching the holder.

    heartbeat stalled + probe starved  -> GIL lock-out, blame a Python thread
    heartbeat stalled + probe fine     -> blocked in C++, the GIL was free

Do NOT attribute a stall from stack-sample residency: a thread parked in a
GIL-releasing native call looks identical to one hogging the GIL.

Run this file directly to validate the probe against known answers before trusting it:
    python tests/perf/gui_stall_probe.py
"""
import os
import sys
import threading
import time

_CLK = os.sysconf("SC_CLK_TCK")


def thread_cpu_s(tid):
    """utime+stime of one thread, in seconds. Distinguishes compute from waiting."""
    try:
        with open(f"/proc/self/task/{tid}/stat") as f:
            fields = f.read().rsplit(")", 1)[1].split()
        return (int(fields[11]) + int(fields[12])) / _CLK
    except OSError:
        return 0.0


def _stack_of(frame, depth):
    out = []
    while frame is not None and len(out) < depth:
        code = frame.f_code
        out.append(f"{code.co_filename}:{frame.f_lineno} in {code.co_name}")
        frame = frame.f_back
    return out


class Heartbeat:
    """GUI-thread liveness. Records every gap over `freeze_ms` with its CPU fraction."""

    def __init__(self, tick_ms=16, freeze_ms=300):
        from PySide6.QtCore import Qt, QTimer

        self.tick_ms = tick_ms
        self.freeze_ms = freeze_ms
        self.lags = []
        self.freezes = []  # (t_start, t_end, lag_ms, cpu_fraction, label)
        self.label = ""
        self._tid = threading.get_native_id()
        self._last = self._last_wall = self._last_cpu = None
        self.timer = QTimer()
        self.timer.setTimerType(Qt.PreciseTimer)
        self.timer.setInterval(tick_ms)
        self.timer.timeout.connect(self._tick)

    def _tick(self):
        now, wall, cpu = time.perf_counter(), time.time(), thread_cpu_s(self._tid)
        if self._last is not None:
            lag = (now - self._last) * 1000 - self.tick_ms
            self.lags.append(lag)
            if lag > self.freeze_ms:
                elapsed = wall - self._last_wall
                frac = (cpu - self._last_cpu) / elapsed if elapsed > 0 else 0.0
                self.freezes.append((self._last_wall, wall, lag, frac, self.label))
        self._last, self._last_wall, self._last_cpu = now, wall, cpu

    def start(self, label=""):
        self.label = label
        self._last = self._last_wall = self._last_cpu = None
        self.lags.clear()
        self.timer.start()

    def stop(self):
        self.timer.stop()

    def report(self, label):
        import numpy as np

        d = np.array(self.lags) if self.lags else np.zeros(1)
        print(f"  {label:22s} lag mean {d.mean():7.1f} ms | median {np.median(d):6.1f} | "
              f"p90 {np.percentile(d, 90):7.1f} | p99 {np.percentile(d, 99):8.1f} | "
              f"max {d.max():9.1f} | >500ms {int((d > 500).sum())} "
              f">100ms {int((d > 100).sum())}", flush=True)


class GILProbe(threading.Thread):
    """Detects GIL lock-out and snapshots the holder at the moment starvation ends."""

    SLICE_S = 0.002

    def __init__(self, threshold_s=0.1, depth=12):
        super().__init__(daemon=True, name="gil-probe")
        self.threshold_s = threshold_s
        self.depth = depth
        self.events = []  # (t_start, t_end, gap_s, {thread_name: frames})
        self._stop = threading.Event()

    def run(self):
        last = time.perf_counter()
        while not self._stop.is_set():
            time.sleep(self.SLICE_S)
            now = time.perf_counter()
            if now - last - self.SLICE_S > self.threshold_s:
                frames = sys._current_frames()
                names = {t.ident: t.name for t in threading.enumerate()}
                snap = {names.get(tid, f"tid-{tid}"): _stack_of(fr, self.depth)
                        for tid, fr in frames.items()}
                self.events.append((time.time() - (now - last), time.time(),
                                    now - last - self.SLICE_S, snap))
            last = time.perf_counter()

    def stop(self):
        self._stop.set()
        self.join(timeout=1)


IDLE_FRAMES = ("threading.py:", "selectors.py:", "connection.py:", "queues.py:")


def attribute(heartbeat, probe):
    """Print each GUI freeze with its cause: GIL lock-out (with holder) or C++ block."""
    from collections import Counter

    print(f"\n=== {len(heartbeat.freezes)} GUI freezes | {len(probe.events)} GIL lock-outs "
          f">{probe.threshold_s * 1000:.0f} ms ===", flush=True)

    for t0, t1, lag, frac, label in sorted(heartbeat.freezes, key=lambda f: -f[2]):
        overlap = [e for e in probe.events if e[1] >= t0 and e[0] <= t1]
        if overlap:
            held = sum(e[2] for e in overlap) * 1000
            verdict = f"GIL LOCK-OUT ({held:.0f} ms of {lag:.0f} ms, {len(overlap)} events)"
        else:
            verdict = "no GIL lock-out -> blocked in C++ (GIL was free)"
        print(f"\n-- {lag:8.1f} ms during {label or '?'}  cpu={frac * 100:5.1f}%\n   {verdict}")
        for _, _, gap, snap in sorted(overlap, key=lambda e: -e[2])[:2]:
            for tname, frames in snap.items():
                if tname in ("gil-probe", "MainThread") or not frames:
                    continue
                if any(k in frames[0] for k in IDLE_FRAMES):
                    continue
                print(f"      [{tname}]")
                for f in frames[:6]:
                    print(f"          {f}")

    if probe.events:
        # Keyed by thread AND frame: the GUI thread can hold the GIL too, so it must
        # stay in the tally but be distinguishable from a worker that starved it.
        print("\n=== lock-outs by thread and innermost non-idle frame ===")
        c = Counter()
        for _, _, _, snap in probe.events:
            for tname, frames in snap.items():
                if tname == "gil-probe" or not frames:
                    continue
                if any(k in frames[0] for k in IDLE_FRAMES):
                    continue
                c[f"{tname:16s} {frames[0].split('/')[-1]}"] += 1
        for f, n in c.most_common(12):
            print(f"  {n:4d}  {f}")


# ---- self-test: known answers, run before trusting the probe -------------------

_BIG_LIST = None


def _long_c_call(_):
    """sorted() on a big list: ONE C call, no yields -> genuinely locks others out.

    A tight pure-Python loop does NOT: CPython preempts it every switch interval
    (5 ms by default), so it never starves anyone for hundreds of ms.
    """
    global _BIG_LIST
    if _BIG_LIST is None:
        import random
        _BIG_LIST = [random.random() for _ in range(12_000_000)]
    return sorted(_BIG_LIST)[0]


def _sleep(duration_s):
    time.sleep(duration_s)


def _numpy_copy(_):
    import numpy as np
    a = np.random.random((30_000_000,))
    b = np.empty_like(a)
    b[:] = a


def _case(name, fn, expect_fire):
    probe = GILProbe(threshold_s=0.1)
    probe.start()
    time.sleep(0.3)
    probe.events.clear()
    worker = threading.Thread(target=fn, args=(0.8,), name="culprit", daemon=True)
    worker.start()
    worker.join()
    time.sleep(0.2)
    probe.stop()

    fired = bool(probe.events)
    worst = max((e[2] for e in probe.events), default=0) * 1000
    ok = fired == expect_fire
    named = ""
    if fired:
        _, _, _, snap = max(probe.events, key=lambda e: e[2])
        frames = snap.get("culprit")
        named = f" -> {frames[0].split('/')[-1]}" if frames else " -> culprit NOT named"
    print(f"  {name:26s} fired={str(fired):5s} (expected {expect_fire})  "
          f"worst {worst:7.1f} ms  {'PASS' if ok else 'FAIL'}{named}")
    return ok


def self_test():
    print("validating GILProbe against known answers:\n")
    results = [
        _case("long C call (sorted)", _long_c_call, True),
        _case("time.sleep", _sleep, False),
        _case("numpy copy", _numpy_copy, False),
    ]
    print(f"\n{'ALL PASS — instrument is trustworthy' if all(results) else 'FAILED — do not trust it'}")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(self_test())

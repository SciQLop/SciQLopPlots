"""Run API fuzz cases out of process and grade each outcome against the contract.

A batch of cases runs in one child interpreter. The child marks each case on stderr (@@SETUP, @@CASE,
@@END sentinels) and prints one JSON line per finished case on stdout, so a crash or a hang names the
case that was running and the cases after it run again in a fresh child.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass

EXPECTS = {"ok", "type", "value", "deleted", "ok_or_value", "script"}


@dataclass(frozen=True)
class Case:
    id: str
    setup: str
    call: str
    expect: str
    allow: tuple[str, ...] = ()  # deliberate refusals: extra accepted exception classes

    def __post_init__(self):
        assert self.expect in EXPECTS, self.expect


@dataclass(frozen=True)
class Outcome:
    case_id: str
    kind: str  # returned | raised | setup_failed | crash | hang
    exc_type: str | None
    message: str | None
    stderr: str
    exc_mro: tuple[str, ...] = ()


@dataclass(frozen=True)
class Verdict:
    case_id: str
    status: str
    detail: str


STDERR_ALLOW = [
    (re.compile(r"^QRhiWidget: (QRhi is not supported on this platform\.|No QRhi)$"),
     "offscreen platform has no QRhi: plots fall back to raster painting"),
    (re.compile(r"^This plugin does not support propagateSizeHints\(\)$"),
     "offscreen platform plugin notice on show()"),
    (re.compile(r'^SciQLopPlots: ".*" cannot draw a colour axis: showing its data without the colour$'),
     "documented degrade: a graph without a colour axis draws a coloured batch's data and says so once"),
    (re.compile(r'^(SciQLopPlots: (dropping data batch from provider for .*|data provider failed: .*)'
                r'|\[GetDataPyCallable\] dropping data provider result: .*'
                r'|.*RuntimeWarning: SciQLopPlots data provider returned .*\(dropping batch\))$'),
     "J1: data arriving later can't raise to the caller; it is printed once and emitted as fetch_failed"),
]

ACCEPTED = {
    "type": {"TypeError"},
    "value": {"ValueError", "OverflowError", "IndexError"},
    "ok_or_value": {"ValueError", "OverflowError", "IndexError"},
    "deleted": {"RuntimeError"},
}
CONTRACT_CLASSES = {"TypeError", "ValueError", "OverflowError", "IndexError", "RuntimeError", "NotImplementedError"}
GENERIC_MESSAGES = {"", "error", "failed", "exception", "an unknown exception was caught"}

_CHILD = r'''
import faulthandler, json, sys, time
faulthandler.enable()
from PySide6.QtWidgets import QApplication
app = QApplication.instance() or QApplication([])
import SciQLopPlots  # noqa: F401  -- first-import output is nobody's case

def pump(seconds=0.2):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        app.processEvents()
        time.sleep(0.005)

def mark(tag, cid):
    sys.stderr.write(f"@@{tag} {cid}@@\n")
    sys.stderr.flush()

def report(cid, kind, e=None):
    print(json.dumps({"id": cid, "kind": kind, "exc_type": e and type(e).__name__, "message": e and str(e),
                      "mro": [c.__name__ for c in type(e).__mro__] if e else []}), flush=True)

for cid, setup, call in CASES:
    ns = {"__name__": "__fuzz__"}
    mark("SETUP", cid)
    faulthandler.dump_traceback_later(PER_CASE_S, exit=True)
    try:
        exec(setup, ns)
        pump()
    except BaseException as e:
        faulthandler.cancel_dump_traceback_later()
        mark("END", cid)
        report(cid, "setup_failed", e)
        continue
    mark("CASE", cid)
    try:
        exec(call, ns)
        kind, err = "returned", None
    except BaseException as e:
        kind, err = "raised", e
    pump()
    faulthandler.cancel_dump_traceback_later()
    mark("END", cid)
    report(cid, kind, err)
    ns.clear()
    pump(0.0)
'''

_SENTINEL = re.compile(r"^@@(SETUP|CASE|END) (.*)@@$")


def _child_source(cases: list[Case], per_case_s: float) -> str:
    rows = [(c.id, c.setup, c.call) for c in cases]
    return f"CASES = {rows!r}\nPER_CASE_S = {per_case_s!r}\n" + _CHILD


def _split_stderr(stderr: str) -> tuple[dict[str, str], str | None, str | None]:
    """Stderr lines per case (call phase only), the last case started, and the phase it was in."""
    per_case: dict[str, list[str]] = {}
    current, phase = None, None
    for line in stderr.splitlines():
        m = _SENTINEL.match(line)
        if m:
            tag, cid = m.groups()
            current, phase = (None, None) if tag == "END" else (cid, tag)
            per_case.setdefault(cid, [])
        elif current is not None and phase == "CASE":
            per_case[current].append(line)
        elif current is not None:
            per_case.setdefault(f"{current}#setup", []).append(line)
    return {k: "\n".join(v) for k, v in per_case.items()}, current, phase


def _run_child(cases: list[Case], per_case_s: float, batch_s: float):
    env = {**os.environ, "QT_QPA_PLATFORM": os.environ.get("QT_QPA_PLATFORM", "offscreen")}
    # Neutral cwd: `python -c` puts the cwd on sys.path, and the source tree would shadow the build.
    try:
        done = subprocess.run([sys.executable, "-c", _child_source(cases, per_case_s)], capture_output=True,
                              text=True, timeout=batch_s, cwd=tempfile.gettempdir(), env=env)
        return done.stdout, done.stderr, done.returncode, False
    except subprocess.TimeoutExpired as e:
        decode = lambda b: b.decode(errors="replace") if isinstance(b, bytes) else (b or "")
        return decode(e.stdout), decode(e.stderr), None, True


def run_batch(cases: list[Case], per_case_s: float = 20, batch_s: float | None = None) -> list[Outcome]:
    """batch_s is only a backstop (each case has its own per_case_s watchdog); by default it scales with the batch."""
    by_id = {c.id: c for c in cases}
    assert len(by_id) == len(cases), "case ids must be unique within a batch"
    outcomes: dict[str, Outcome] = {}
    pending = list(cases)
    while pending:
        stdout, stderr, returncode, timed_out = _run_child(pending, per_case_s,
                                                           batch_s or per_case_s * len(pending) + 30)
        per_case, last, phase = _split_stderr(stderr)
        for line in stdout.splitlines():
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(row, dict) and row.get("id") in by_id:
                outcomes[row["id"]] = Outcome(row["id"], row["kind"], row["exc_type"], row["message"],
                                              per_case.get(row["id"], ""), tuple(row["mro"]))
        culprit = last if last is not None and last not in outcomes else None
        if culprit is not None:
            hung = timed_out or "Timeout (" in stderr
            where = per_case.get(culprit, "") or per_case.get(f"{culprit}#setup", "")
            detail = ("during setup\n" if phase == "SETUP" else "") + where + f"\nexit code {returncode}"
            outcomes[culprit] = Outcome(culprit, "hang" if hung else "crash", None, None, detail.strip())
        elif returncode != 0 or timed_out:
            # The child died outside any case (at import or teardown): blame every unfinished case.
            for c in pending:
                outcomes.setdefault(c.id, Outcome(c.id, "crash", None, None,
                                                  f"child died outside a case (exit {returncode})\n{stderr[-2000:]}"))
        pending = [c for c in pending if c.id not in outcomes]
    return [outcomes[c.id] for c in cases]


def _unexplained_stderr(stderr: str) -> str:
    return "\n".join(line for line in stderr.splitlines()
                     if line.strip() and not any(p.match(line) for p, _ in STDERR_ALLOW))


def classify(case: Case, outcome: Outcome) -> Verdict:
    v = lambda status, detail="": Verdict(case.id, status, detail)
    if outcome.kind in ("crash", "hang"):
        return v(outcome.kind, outcome.stderr)
    if outcome.kind == "setup_failed":
        return v("baseline_broken", f"setup raised {outcome.exc_type}: {outcome.message}")
    noise = _unexplained_stderr(outcome.stderr)
    if noise and outcome.kind == "returned":
        return v("swallowed", noise)
    raised = f"{outcome.exc_type}: {outcome.message}" + (f"\n(also printed: {noise})" if noise else "")
    if case.expect in ("ok", "script"):
        return v("pass") if outcome.kind == "returned" else v("baseline_broken", raised)
    if outcome.kind == "returned":
        return v("pass") if case.expect == "ok_or_value" else v("no_error", f"expected {case.expect}")
    accepted = ACCEPTED[case.expect] | set(case.allow)
    if not accepted & set(outcome.exc_mro or (outcome.exc_type,)):
        known = outcome.exc_type in CONTRACT_CLASSES
        return v("wrong_class" if known else "unexpected_class", f"expected {case.expect}, got {raised}")
    if (outcome.message or "").strip().lower() in GENERIC_MESSAGES:
        return v("bad_message", raised)
    return v("pass")

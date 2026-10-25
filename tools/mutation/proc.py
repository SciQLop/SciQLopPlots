"""Commands run in their own process group, so a timeout or an interrupt kills
everything they started (ninja's compilers, pytest's children), not only the
direct child."""
import os
import signal
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path


class Interrupted(Exception):
    pass


def _raise_interrupted(signum, frame):
    raise Interrupted(signal.Signals(signum).name)


def install_signal_handlers():
    """SIGTERM (from `timeout`), SIGHUP and SIGINT unwind through the finally blocks that restore sources."""
    for sig in (signal.SIGTERM, signal.SIGHUP, signal.SIGINT):
        signal.signal(sig, _raise_interrupted)


@dataclass
class Result:
    code: int | None
    output: str
    seconds: float

    @property
    def timed_out(self):
        return self.code is None


def _kill_group(child):
    try:
        os.killpg(child.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def run(cmd, *, cwd=None, env=None, timeout, group_file=None):
    start = time.monotonic()
    child = subprocess.Popen(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors="replace", start_new_session=True)
    if group_file:
        Path(group_file).write_text(str(child.pid))
    try:
        output, _ = child.communicate(timeout=timeout)
        return Result(child.returncode, output, time.monotonic() - start)
    except subprocess.TimeoutExpired:
        _kill_group(child)
        output, _ = child.communicate()
        return Result(None, output, time.monotonic() - start)
    finally:
        if child.poll() is None:
            _kill_group(child)
            child.wait()
        if group_file:
            Path(group_file).unlink(missing_ok=True)


def kill_leftover(group_file):
    """Kills the group a SIGKILLed run left behind: in its own session, its build or test outlives the kill."""
    # simplify: trusts the recorded id; the ceiling is a reused process-group id killed by
    # mistake. Upgrade: store the start time from /proc/<pid>/stat and compare it first.
    path = Path(group_file)
    if not path.exists():
        return
    try:
        os.killpg(int(path.read_text()), signal.SIGKILL)
    except (ProcessLookupError, ValueError):
        pass
    path.unlink()

import subprocess
import sys
import time
from pathlib import Path

import proc


def _gone(pid, wait=5.0):
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        try:
            if Path(f"/proc/{pid}/stat").read_text().split(") ", 1)[1].startswith("Z"):
                return True
        except (FileNotFoundError, ProcessLookupError):
            return True
        time.sleep(0.05)
    return False


def test_result_keeps_output_and_exit_code():
    result = proc.run(["bash", "-c", "echo hello; echo oops >&2; exit 3"], timeout=10)
    assert result.code == 3 and not result.timed_out
    assert "hello" in result.output and "oops" in result.output


def test_timeout_kills_the_whole_process_group(tmp_path):
    pidfile = tmp_path / "pid"
    result = proc.run(["bash", "-c", f"sleep 60 & echo $! > {pidfile}; wait"], timeout=1)
    assert result.timed_out
    assert _gone(int(pidfile.read_text()))


def test_run_records_its_group_while_running_and_clears_it_after(tmp_path):
    group = tmp_path / "child.pgid"
    result = proc.run(["bash", "-c", f"sleep 0.3; cat {group}"], timeout=10, group_file=group)
    assert result.output.strip().isdigit()
    assert not group.exists()


def test_kill_leftover_kills_the_recorded_group(tmp_path):
    group = tmp_path / "child.pgid"
    child = subprocess.Popen(["sleep", "60"], start_new_session=True)
    group.write_text(str(child.pid))
    proc.kill_leftover(group)
    assert child.wait(timeout=5) == -9
    assert not group.exists()
    proc.kill_leftover(group)


def test_sigterm_becomes_interrupted():
    tool_dir = str(Path(proc.__file__).parent)
    code = (
        f"import os, signal, sys; sys.path.insert(0, {tool_dir!r})\n"
        "import proc\n"
        "proc.install_signal_handlers()\n"
        "try:\n"
        "    os.kill(os.getpid(), signal.SIGTERM)\n"
        "except proc.Interrupted as why:\n"
        "    print('caught', why)\n"
    )
    out = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=10).stdout
    assert out.strip() == "caught SIGTERM"

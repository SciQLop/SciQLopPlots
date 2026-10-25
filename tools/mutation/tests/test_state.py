import signal
import subprocess
import sys
import time
from pathlib import Path

import pytest

import state as st

TOOL_DIR = Path(st.__file__).resolve().parent
ORIGINAL = "int f() { return 1 < 2; }\n"


@pytest.fixture
def files(tmp_path):
    src = tmp_path / "f.cpp"
    src.write_text(ORIGINAL)
    return src, tmp_path / "run" / "state.json", tmp_path / "run" / "backup"


def test_save_is_atomic_and_load_fills_defaults(files):
    _, state_path, _ = files
    st.save(state_path, {"mutants": {"a": 1}})
    data = st.load(state_path)
    assert data["mutants"] == {"a": 1} and data["in_flight"] == []
    assert not state_path.with_name("state.json.tmp").exists()


def test_mutated_writes_then_restores(files):
    src, state_path, backup = files
    with st.mutated(state_path, st.new_state(), src, "MUTANT\n", backup):
        assert src.read_text() == "MUTANT\n"
        assert st.load(state_path)["in_flight"][0]["path"] == str(src)
    assert src.read_text() == ORIGINAL
    assert st.load(state_path)["in_flight"] == []


def test_mutated_restores_on_exception(files):
    src, state_path, backup = files
    with pytest.raises(RuntimeError, match="boom"):
        with st.mutated(state_path, st.new_state(), src, "MUTANT\n", backup):
            raise RuntimeError("boom")
    assert src.read_text() == ORIGINAL


def _child(src, state_path, backup, then):
    code = (
        f"import os, signal, sys, time; sys.path.insert(0, {str(TOOL_DIR)!r})\n"
        "import proc, state as st\n"
        "proc.install_signal_handlers()\n"
        f"with st.mutated({str(state_path)!r}, st.new_state(), {str(src)!r}, 'MUTANT\\n', {str(backup)!r}):\n"
        f"    {then}\n"
    )
    return subprocess.Popen([sys.executable, "-c", code])


def _wait_for_mutant(src):
    deadline = time.monotonic() + 10
    while src.read_text() != "MUTANT\n":
        assert time.monotonic() < deadline, "the child never wrote the mutant"
        time.sleep(0.05)


def test_a_killed_run_leaves_a_leftover_that_restore_puts_back(files):
    src, state_path, backup = files
    _child(src, state_path, backup, "os.kill(os.getpid(), signal.SIGKILL)").wait(timeout=10)
    assert src.read_text() == "MUTANT\n"
    data = st.load(state_path)
    assert st.restore_leftovers(state_path, data) == [str(src)]
    assert src.read_text() == ORIGINAL
    assert st.load(state_path)["in_flight"] == []


def test_sigterm_inside_the_block_restores(files):
    src, state_path, backup = files
    child = _child(src, state_path, backup, "time.sleep(30)")
    _wait_for_mutant(src)
    child.send_signal(signal.SIGTERM)
    assert child.wait(timeout=10) != 0
    assert src.read_text() == ORIGINAL
    assert st.load(state_path)["in_flight"] == []

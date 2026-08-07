"""A wrong-argument call must raise a catchable Python error, never abort.

Shiboken formats the "Supported signatures:" part of an argument-mismatch
TypeError by evaluating each parameter type string in
shibokensupport.signature.mapping's namespace. Our ``<primitive-type>``
declarations are not wrapped classes, so an unregistered one resolves to a bare
``str``; shiboken's matcher then calls ``the_type.__module__`` on it, which
raises *inside* the error handler and makes libshiboken call ``Py_FatalError``.
The process dies with no traceback the caller can catch, so these run out of
process.
"""
import subprocess
import sys
import textwrap

import pytest

PREAMBLE = """
import SciQLopPlots  # noqa: F401  -- installs the signature type map
from PySide6 import QtWidgets
app = QtWidgets.QApplication([])
"""

# One entry per <primitive-type> declared in bindings.xml that appears in a
# public signature. Each call deliberately mismatches that parameter.
BAD_CALLS = {
    "std::size_t": "SciQLopPlots.SciQLopNDProjectionPlot(None, 3)",
    "std::string": "SciQLopPlots.tracing_set_thread_name(object())",
    "long": "SciQLopPlots.validate_index(object(), 1, 'x')",
    "SciQLopPyBuffer": "SciQLopPlots.validate_buffer(object(), 'x')",
    "GetDataPyCallable": "SciQLopPlots.SciQLopPlot().line(42)",
}


def run_snippet(body, tmp_path):
    """Run `body` in a fresh interpreter, return (returncode, stdout, stderr).

    The child runs from an empty directory on purpose. `python -c` prepends its
    working directory to sys.path, so inheriting the parent's cwd meant that
    running pytest from the repo root — which is what CI does — put the *source*
    ``SciQLopPlots/`` package first. It has an ``__init__.py`` but no compiled
    ``SciQLopPlotsBindings``, so every child died with an ImportError before
    reaching the call under test. The package itself is found through the
    inherited environment, exactly as the parent found it.
    """
    proc = subprocess.run(
        [sys.executable, "-c", PREAMBLE + textwrap.dedent(body)],
        capture_output=True, text=True, cwd=tmp_path)
    return proc.returncode, proc.stdout, proc.stderr


def test_the_child_interpreter_imports_the_built_package(tmp_path):
    """Guard the harness itself: a broken import would fail every test below
    for a reason that has nothing to do with what they assert."""
    code, out, err = run_snippet('print("OK:" + SciQLopPlots.__file__)', tmp_path)
    assert code == 0, err
    assert out.startswith("OK:"), err


@pytest.mark.parametrize("call", BAD_CALLS.values(), ids=list(BAD_CALLS))
def test_wrong_argument_stays_catchable(call, tmp_path):
    code, out, err = run_snippet(
        f"""
        try:
            {call}
        except Exception as e:
            print("RAISED:" + type(e).__name__)
        """,
        tmp_path,
    )
    assert "Fatal Python error" not in err, f"interpreter aborted:\n{err}"
    assert code == 0, f"exited {code}:\n{err}"
    assert out.startswith("RAISED:"), f"nothing raised, got {out!r} {err!r}"


def test_signature_mismatch_reports_supported_signatures(tmp_path):
    """The TypeError must carry shiboken's listing, not a masked NameError."""
    code, out, err = run_snippet(
        """
        try:
            SciQLopPlots.SciQLopNDProjectionPlot(None, 3)
        except TypeError as e:
            print("MSG:" + str(e).replace(chr(10), " | "))
        """,
        tmp_path,
    )
    assert code == 0, err
    assert "Supported signatures" in out, f"no signature listing in: {out!r}"

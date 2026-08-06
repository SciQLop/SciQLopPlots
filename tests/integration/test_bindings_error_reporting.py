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


def run_snippet(body):
    """Run `body` in a fresh interpreter, return (returncode, stdout, stderr)."""
    proc = subprocess.run(
        [sys.executable, "-c", PREAMBLE + textwrap.dedent(body)],
        capture_output=True, text=True)
    return proc.returncode, proc.stdout, proc.stderr


@pytest.mark.parametrize("call", BAD_CALLS.values(), ids=list(BAD_CALLS))
def test_wrong_argument_stays_catchable(call):
    code, out, err = run_snippet(
        f"""
        try:
            {call}
        except Exception as e:
            print("RAISED:" + type(e).__name__)
        """
    )
    assert "Fatal Python error" not in err, f"interpreter aborted:\n{err}"
    assert code == 0, f"exited {code}:\n{err}"
    assert out.startswith("RAISED:"), f"nothing raised, got {out!r} {err!r}"


def test_signature_mismatch_reports_supported_signatures():
    """The TypeError must carry shiboken's listing, not a masked NameError."""
    code, out, err = run_snippet(
        """
        try:
            SciQLopPlots.SciQLopNDProjectionPlot(None, 3)
        except TypeError as e:
            print("MSG:" + str(e).replace(chr(10), " | "))
        """
    )
    assert code == 0, err
    assert "Supported signatures" in out, f"no signature listing in: {out!r}"

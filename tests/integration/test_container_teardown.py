"""A panel deleted while deleting its plots runs the event loop (the browser does, waiting for their
worker threads): the calls its plots queued must not reach the half-destroyed panel."""
import os
import subprocess
import sys
import tempfile

SCRIPT = """
import numpy as np, shiboken6
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QCoreApplication
app = QApplication([])
from SciQLopPlots import SciQLopMultiPlotPanel
panel = SciQLopMultiPlotPanel()
x = np.linspace(0, 10, 100)
plots = [panel.plot(x, np.sin(x))[0] for _ in range(3)]
for p in plots:
    p.destroyed.connect(lambda *_: QCoreApplication.processEvents())
app.processEvents()
del plots
shiboken6.delete(panel)
print("teardown ok")
"""


def test_a_panel_deleted_with_its_plots_ignores_their_queued_calls():
    # Neutral cwd: `python -c` puts the cwd on sys.path, and from the repo root the
    # source package (no compiled bindings) would shadow the built one.
    done = subprocess.run([sys.executable, "-c", SCRIPT], capture_output=True, text=True, timeout=120,
                          cwd=tempfile.gettempdir(), env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
    assert (done.returncode, done.stdout.strip()) == (0, "teardown ok"), done.stderr[-1000:]

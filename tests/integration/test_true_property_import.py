"""Importing SciQLopPlots under true_property, after a QApplication exists, must not abort Python."""
import subprocess
import sys
import textwrap

SCRIPT = textwrap.dedent("""
    import PySide6
    from __feature__ import true_property
    from PySide6.QtWidgets import QApplication
    app = QApplication([])
    import SciQLopPlots
    plot = SciQLopPlots.SciQLopPlot()
    assert plot.selected is False and plot.name == plot.objectName
    print("ok")
""")


def test_import_after_qapplication_under_true_property():
    run = subprocess.run([sys.executable, "-c", SCRIPT], capture_output=True, text=True, timeout=120,
                         env={**__import__("os").environ, "QT_QPA_PLATFORM": "offscreen"})
    assert (run.returncode, run.stdout.strip()) == (0, "ok"), run.stderr[-2000:]

"""A theme parented to the plot it styles must not crash the plot's destruction.

SciQLopTheme.dark(plot) makes the theme a child of the plot. Deleting the plot
deletes its children, the theme among them, and the theme's destroyed() signal
reached SciQLopPlot::set_theme's handler after the plot's inner widget was gone:
a segfault on every close. Run in a subprocess so a regression fails this test
instead of killing the whole suite.
"""
import subprocess
import sys
import tempfile
import textwrap

SCRIPT = textwrap.dedent("""
    import numpy as np
    import shiboken6
    from PySide6.QtWidgets import QApplication
    app = QApplication([])
    from SciQLopPlots import SciQLopPlot, SciQLopTheme

    plot = SciQLopPlot()
    x = np.linspace(0, 10, 100)
    plot.plot(x, np.sin(x), labels=["sin"])
    plot.set_theme(SciQLopTheme.dark(plot))
    app.processEvents()
    shiboken6.delete(plot)
    app.processEvents()
    print("survived")
""")


def test_deleting_a_plot_with_a_theme_it_parents():
    # Neutral cwd: `python -c` puts the cwd on sys.path, and from the repo root the
    # source package (no compiled bindings) would shadow the built one.
    run = subprocess.run([sys.executable, "-c", SCRIPT], capture_output=True, text=True,
                         timeout=60, cwd=tempfile.gettempdir())
    assert run.returncode == 0 and "survived" in run.stdout, run.stderr[-2000:]

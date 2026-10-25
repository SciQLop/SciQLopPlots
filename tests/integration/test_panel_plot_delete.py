"""shiboken6.delete() of a plot a panel factory returned must not crash the panel's teardown.

The failure is a segfault at interpreter level, so each scenario runs in a child process.
"""
import os
import subprocess
import sys

import pytest

SCRIPT = """
import numpy as np, shiboken6
from PySide6.QtWidgets import QApplication
app = QApplication([])
from SciQLopPlots import SciQLopMultiPlotPanel, PlotType
x = np.linspace(0.0, 10.0, 200)
panel = SciQLopMultiPlotPanel()
result = {factory}
plot = result[0]
shiboken6.delete(plot)
del plot, result
app.processEvents()
shiboken6.delete(panel) if {delete_panel} else None
del panel
print("teardown ok")
"""

FACTORIES = {
    "line": "panel.plot(x, x)",
    "timeseries": "panel.plot(x, x, plot_type=PlotType.TimeSeries)",
    "colormap": "panel.plot(x, np.arange(8.0), np.random.rand(200, 8))",
    "inserted": "(panel.plot(x, x), panel.plot(x, x, index=0))[1]",
    "histogram": "panel.histogram(x)",
    "projection": "panel.plot(x, np.column_stack([np.sin(x), np.cos(x), x]), plot_type=PlotType.Projections)",
}


def run_scenario(factory, delete_panel):
    code = SCRIPT.format(factory=FACTORIES[factory], delete_panel=delete_panel)
    return subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                          timeout=120, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})


@pytest.mark.parametrize("delete_panel", [False, True], ids=["del", "shiboken-delete"])
@pytest.mark.parametrize("factory", FACTORIES)
def test_deleting_a_factory_plot_then_the_panel_does_not_crash(factory, delete_panel):
    done = run_scenario(factory, delete_panel)
    assert done.returncode == 0, (done.returncode, done.stderr[-500:])
    assert "teardown ok" in done.stdout


def test_python_built_plot_baseline_still_fine():
    code = SCRIPT.format(factory="(__import__('SciQLopPlots').SciQLopPlot(),)", delete_panel=False)
    code = code.replace("plot = result[0]", "plot = result[0]; panel.add_plot(plot)")
    done = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=120,
                          env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
    assert done.returncode == 0, done.stderr[-500:]

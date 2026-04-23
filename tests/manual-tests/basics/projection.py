"""3D projection plot with time-colored helix.

Showcases:
- SciQLopNDProjectionPlot for multi-dimensional phase-space visualization
- Time-color encoding (blue→red gradient along trajectory)
- Reference curves (static context overlays)
- Equal aspect ratio for undistorted spatial data
- Per-subplot axis labels
- Linked crosshairs across subplots
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopMultiPlotPanel, PlotType, GraphType,
)

app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=False)
panel.setWindowTitle("Projection Plot Demo")

# Helix trajectory in 3D
t = np.linspace(0, 6 * np.pi, 2000, dtype=np.float64)
x = 10 * np.cos(t)
y = 10 * np.sin(t)
z = t

proj, _ = panel.plot(
    [t, x, y, z],
    labels=["helix"] * 3,
    graph_type=GraphType.ParametricCurve,
    plot_type=PlotType.Projections,
)

proj.set_axis_labels(["X", "Y", "Z"])
proj.set_equal_aspect_ratio(True)
proj.set_time_color_enabled(True)
proj.set_time_color_gradient(QColor("blue"), QColor("red"))
proj.set_linked_crosshairs(True)

# Reference circle at r=10 in the XY plane
theta = np.linspace(0, 2 * np.pi, 200, dtype=np.float64)
proj.add_reference_curve(
    [10 * np.cos(theta), 10 * np.sin(theta), np.zeros_like(theta)],
    label="r=10",
    color=QColor(100, 100, 100),
)

panel.show()
panel.resize(1200, 500)
sys.exit(app.exec())

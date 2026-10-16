"""A Qt Graphs 3-D surface as a row of a SciQLopPlots panel.

Showcases:
- WidgetPlot: any QWidget in a panel, following the panel's time range
- The spectrogram in view, as a surface: time, energy and flux
"""
import sys

import numpy as np
import shiboken6
from PySide6.QtGui import QColor, QLinearGradient, QVector3D
from PySide6.QtGraphs import QGraphsTheme, QSurface3DSeries, QSurfaceDataItem, QtGraphs3D
from PySide6.QtGraphsWidgets import Q3DSurfaceWidgetItem
from PySide6.QtQuickWidgets import QQuickWidget
from PySide6.QtWidgets import QApplication

from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, WidgetPlot

t = 1.7e9 + np.linspace(0, 6 * 3600, 2000)              # six hours, seconds since 1970
energy = np.logspace(1, 4, 32)                           # 32 channels, eV
peak = 2 + np.sin(t / 2000)                              # log10 of the peak energy drifts
flux = np.exp(-((np.log10(energy)[None, :] - peak[:, None]) ** 2) / 0.1)   # (time, energy)

app = QApplication(sys.argv)
panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("Qt Graphs in a panel")
panel.set_time_axis_range(t[0], t[-1])
panel.plot(t, energy, flux, y_log_scale=True, plot_type=PlotType.TimeSeries)

view = QQuickWidget()
view.setResizeMode(QQuickWidget.ResizeMode.SizeRootObjectToView)
surface = Q3DSurfaceWidgetItem()
surface.setWidget(view)
surface.setCameraPreset(QtGraphs3D.CameraPreset.IsometricRightHigh)
surface.setHorizontalAspectRatio(1.0)                  # as deep in energy as long in time
surface.setCameraZoomLevel(125)
series = QSurface3DSeries()
series.setDrawMode(QSurface3DSeries.DrawFlag.DrawSurface)
gradient = QLinearGradient()
for stop, color in ((0, "navy"), (0.35, "cyan"), (0.65, "yellow"), (1, "darkred")):
    gradient.setColorAt(stop, QColor(color))
series.setBaseGradient(gradient)
series.setColorStyle(QGraphsTheme.ColorStyle.RangeGradient)
surface.addSeries(series)
surface.axisX().setTitle("time (min from start of view)")
surface.axisZ().setTitle("log10 energy")
surface.axisY().setTitle("flux")
for axis in (surface.axisX(), surface.axisY(), surface.axisZ()):
    axis.setTitleVisible(True)


def surface_of_view(start, stop, max_rows=120):
    rows = np.flatnonzero((t >= start) & (t <= stop))
    if rows.size < 2:
        return
    rows = rows[:: max(1, rows.size // max_rows)]          # a surface needs few rows
    minutes = (t[rows] - start) / 60
    # One surface row per energy (constant z), along time (growing x).
    series.dataProxy().resetArray([
        [QSurfaceDataItem(QVector3D(m, f, e)) for m, f in zip(minutes, flux[rows, j])]
        for j, e in enumerate(np.log10(energy))
    ])


panel.add_plot(WidgetPlot(view, on_time_range=surface_of_view))
# A Qt Graphs item must go before its QQuickWidget, which the panel deletes last: delete
# the item while the window still exists.
app.aboutToQuit.connect(lambda: shiboken6.delete(surface))
panel.resize(900, 700)
panel.show()
sys.exit(app.exec())

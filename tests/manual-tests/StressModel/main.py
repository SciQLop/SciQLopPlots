
"""
The idea of this test is to create an delete continously plot panels until it
 crashes in order to catch race conditions or wrong destruction order.
"""

from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget
from PySide6.QtGui import QColorConstants, QColor
from PySide6.QtCore import Qt, QTimer
import sys, os
import numpy as np
from datetime import datetime, timezone
import threading
from typing import Dict, Any
from SciQLopPlots import (SciQLopPlot, MultiPlotsVerticalSpan,SciQLopMultiPlotPanel, SciQLopVerticalSpan,
                         SciQLopTimeSeriesPlot, GraphType, PlotType, AxisType, SciQLopPlotRange, PlotsModel,
                         InspectorView, SciQLopPixmapItem, SciQLopEllipseItem,
                         Coordinates)
import random


sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow


os.environ['QT_API'] = 'PySide6'

NPOINTS = 30000


def make_plot(parent, time_axis=False):
    if time_axis:
        plot = SciQLopTimeSeriesPlot(parent)
        plot.x_axis().set_range(datetime.now().timestamp(), datetime.now().timestamp()+NPOINTS)
    else:
        plot = SciQLopPlot(parent)
        plot.x_axis().set_range(SciQLopPlotRange(0, NPOINTS))
    plot.enable_legend(True)
    plot.minimize_margins()
    return plot


def add_graph(plot, time_axis=False, offset=0.):
    r = plot.x_axis().range()
    x=np.arange(start=r.start(), stop=r.stop(), dtype=np.float64)
    y=np.cos(np.array([x/6.,x/60.,x/600.])) * np.cos(np.array([x,x/6.,x/60.])) * [[100.],[1300],[17000]]
    y+=offset
    y=np.array(y.T, copy=True)
    return plot.plot(x,y, labels=["X","Y","Z"], colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green])


class StackedPlots(SciQLopMultiPlotPanel):
    def __init__(self, parent):
        SciQLopMultiPlotPanel.__init__(self, parent)
        self.setMouseTracking(True)
        self.graphs = []
        for _ in range(3):
            plot = make_plot(None, time_axis=True)
            self.add_plot(plot)
            self.graphs.append(add_graph(plot, time_axis=True))

        x_range = self.plot_at(0).x_axis().range()
        self._sub_panel = SciQLopMultiPlotPanel(self,
                                               synchronize_x=False,
                                               synchronize_time=True,
                                               orientation=Qt.Orientation.Horizontal)
        self.add_panel(self._sub_panel)
        for _ in range(2):
            plot = make_plot(None, time_axis=True)
            self._sub_panel.add_plot(plot)
            self.graphs.append(add_graph(plot, time_axis=True))
        self._verticalSpan = MultiPlotsVerticalSpan(self, x_range/10, QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="Vertical Span")
        self.setAttribute(Qt.WA_DeleteOnClose)


def create_plot_panel(main_window):
    main_window.tabs.add_tab(StackedPlots(main_window), f"StackedPlots-{main_window.tabs.count()}")


def remove_plot_panel(main_window):
    if main_window.tabs.count() == 0:
        return
    random_tab_index = np.random.randint(0, main_window.tabs.count())
    main_window.tabs.widget(random_tab_index).close()


def random_add_or_remove_panels(main_window):
    random.choices([create_plot_panel, remove_plot_panel],
                   weights=[10, main_window.tabs.count()])[0](main_window)


if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w._timer = QTimer(w)
    w._timer.timeout.connect(lambda: random_add_or_remove_panels(w))
    w._timer.setInterval(100)
    w._timer.start()
    create_plot_panel(w)
    w.add_to_kernel_namespace("create_plot_panel", lambda: create_plot_panel(w))
    w.add_to_kernel_namespace("remove_plot_panel", lambda index: w.tabs.widget(index).close())
    w.add_to_kernel_namespace("stop_timer", w._timer.stop)
    w.add_to_kernel_namespace("start_timer",  w._timer.start)
    w.show()
    app.exec()

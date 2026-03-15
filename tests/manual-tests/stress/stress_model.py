"""Random create/destroy plot panels for stability testing.

Showcases:
- Rapid panel lifecycle stress testing
- Random weighted create/remove operations
"""
import sys
import random
import numpy as np
from datetime import datetime
from PySide6.QtWidgets import QApplication, QMainWindow, QTabWidget
from PySide6.QtGui import QColorConstants, QColor
from PySide6.QtCore import Qt, QTimer

from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopTimeSeriesPlot,
    MultiPlotsVerticalSpan, SciQLopPlotRange, PlotType,
)


def make_panel(parent):
    panel = SciQLopMultiPlotPanel(parent, synchronize_x=False, synchronize_time=True)

    def make_data(start, stop):
        x = np.arange(start, stop, dtype=np.float64)
        y = np.column_stack([
            np.cos(x / 6) * np.cos(x) * 100,
            np.cos(x / 60) * np.cos(x / 6) * 1300,
            np.cos(x / 600) * np.cos(x / 60) * 17000,
        ])
        return x, y

    for _ in range(3):
        panel.plot(
            make_data,
            labels=["X", "Y", "Z"],
            colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
            plot_type=PlotType.TimeSeries,
        )

    x_range = panel.plot_at(0).x_axis().range()
    MultiPlotsVerticalSpan(
        panel, x_range / 10,
        QColor(100, 100, 100, 100),
        read_only=False, visible=True,
        tool_tip="Vertical Span",
    )
    panel.setAttribute(Qt.WA_DeleteOnClose)
    return panel


class StressWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Stress Test — random create/destroy")
        self.tabs = QTabWidget(self)
        self.tabs.setMovable(True)
        self.setCentralWidget(self.tabs)
        self.setGeometry(200, 200, 800, 600)

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._random_op)
        self._timer.setInterval(100)
        self._timer.start()
        self._create()

    def _create(self):
        panel = make_panel(self)
        self.tabs.addTab(panel, f"Panel-{self.tabs.count()}")

    def _remove(self):
        if self.tabs.count() == 0:
            return
        idx = random.randint(0, self.tabs.count() - 1)
        self.tabs.widget(idx).close()

    def _random_op(self):
        random.choices(
            [self._create, self._remove],
            weights=[10, max(1, self.tabs.count())],
        )[0]()


app = QApplication(sys.argv)
w = StressWindow()
w.show()
sys.exit(app.exec())

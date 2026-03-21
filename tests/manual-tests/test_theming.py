"""SciQLopPlots Theming Demo — toggle between light and dark themes."""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QPushButton, QHBoxLayout
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopTheme,
    PlotType, SciQLopPlotRange,
)


def sample_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([np.sin(x / 50), np.cos(x / 30)])
    return x, y


def main():
    app = QApplication(sys.argv)
    win = QMainWindow()
    central = QWidget()
    layout = QVBoxLayout(central)

    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    panel.plot(sample_data, labels=["sin", "cos"], plot_type=PlotType.TimeSeries)
    panel.plot(sample_data, labels=["sin", "cos"], plot_type=PlotType.TimeSeries)

    dark_theme = SciQLopTheme.dark()
    light_theme = SciQLopTheme.light()
    is_dark = [False]

    def toggle():
        is_dark[0] = not is_dark[0]
        panel.set_theme(dark_theme if is_dark[0] else light_theme)
        btn.setText("Switch to Light" if is_dark[0] else "Switch to Dark")

    btn_layout = QHBoxLayout()
    btn = QPushButton("Switch to Dark")
    btn.clicked.connect(toggle)
    btn_layout.addWidget(btn)

    # Per-plot override demo: get the first plot from the panel's plots list
    custom_theme = SciQLopTheme.dark()
    custom_theme.set_background(QColor("#1a3a1a"))
    custom_theme.set_selection(QColor("#00ff88"))

    def apply_custom():
        plots = panel.plots()
        if plots:
            plots[0].set_theme(custom_theme)

    btn2 = QPushButton("Custom Theme on Plot 0")
    btn2.clicked.connect(apply_custom)
    btn_layout.addWidget(btn2)

    layout.addLayout(btn_layout)
    layout.addWidget(panel)
    win.setCentralWidget(central)
    win.resize(900, 600)
    win.show()

    panel.set_x_axis_range(SciQLopPlotRange(0, 500))

    sys.exit(app.exec())


if __name__ == "__main__":
    main()

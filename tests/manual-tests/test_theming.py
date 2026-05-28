"""SciQLopPlots Theming Demo — toggle between light and dark themes.

Also serves as a reproducer for the selection-border theming bug:
``SciQLopPlotInterface::set_selected`` hardcoded ``border: 2px solid black``,
which is invisible in dark mode. The selection border must adopt the active
theme's ``selection()`` color. ``check_selection_border_follows_theme`` runs
before the GUI and fails loudly if the regression returns.
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QPushButton, QHBoxLayout
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopPlot, SciQLopTheme,
    PlotType, SciQLopPlotRange,
)


def sample_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([np.sin(x / 50), np.cos(x / 30)])
    return x, y


def check_selection_border_follows_theme():
    plot = SciQLopPlot()
    try:
        for factory in (SciQLopTheme.light, SciQLopTheme.dark):
            theme = factory()
            plot.set_theme(theme)
            expected = theme.selection().name().lower()
            plot.set_selected(True)
            stylesheet = plot.styleSheet().lower()
            assert expected in stylesheet, (
                f"selection border did not adopt theme color {expected}; "
                f"got stylesheet={stylesheet!r}"
            )
            assert "black" not in stylesheet, (
                f"selection border still contains hardcoded 'black' under "
                f"{factory.__name__} theme; stylesheet={stylesheet!r}"
            )
            plot.set_selected(False)

        theme = SciQLopTheme.dark()
        plot.set_theme(theme)
        plot.set_selected(True)
        recolored = QColor("#ff00aa")
        theme.set_selection(recolored)
        assert recolored.name().lower() in plot.styleSheet().lower(), (
            "selection border did not refresh after theme.set_selection(); "
            f"stylesheet={plot.styleSheet()!r}"
        )
    finally:
        plot.deleteLater()


def main():
    app = QApplication(sys.argv)

    check_selection_border_follows_theme()

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

    def toggle_selection():
        plots = panel.plots()
        if plots:
            plots[0].set_selected(not plots[0].selected())

    btn3 = QPushButton("Toggle Plot 0 Selection")
    btn3.clicked.connect(toggle_selection)
    btn_layout.addWidget(btn3)

    layout.addLayout(btn_layout)
    layout.addWidget(panel)
    win.setCentralWidget(central)
    win.resize(900, 600)
    win.show()

    panel.set_x_axis_range(SciQLopPlotRange(0, 500))

    sys.exit(app.exec())


if __name__ == "__main__":
    main()

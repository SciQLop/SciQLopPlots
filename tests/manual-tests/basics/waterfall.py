"""Waterfall graph demo — channel stack and record section."""
import sys
import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QApplication, QWidget, QVBoxLayout, QHBoxLayout,
                                QTabWidget, QSlider, QCheckBox, QLabel)
from SciQLopPlots import SciQLopPlot, GraphType


def make_channel_stack_tab():
    n_channels = 8
    n_samples = 2000
    t = np.linspace(0, 10, n_samples).astype(np.float64)
    rng = np.random.default_rng(42)
    y = np.zeros((n_samples, n_channels), dtype=np.float64)
    for i in range(n_channels):
        f = 0.5 + 0.3 * i
        y[:, i] = np.sin(2 * np.pi * f * t) + 0.15 * rng.standard_normal(n_samples)

    widget = QWidget()
    layout = QVBoxLayout(widget)
    plot = SciQLopPlot()
    layout.addWidget(plot)

    wf = plot.plot(t, y, graph_type=GraphType.Waterfall,
                   offsets=2.5, normalize=True, gain=1.0,
                   labels=[f"ch{i}" for i in range(n_channels)])

    gain_row = QHBoxLayout()
    gain_row.addWidget(QLabel("Gain"))
    gain = QSlider(Qt.Horizontal)
    gain.setRange(10, 500)
    gain.setValue(100)
    gain_label = QLabel("1.00")
    def on_gain(v):
        g = v / 100.0
        wf.set_gain(g)
        gain_label.setText(f"{g:.2f}")
    gain.valueChanged.connect(on_gain)
    gain_row.addWidget(gain)
    gain_row.addWidget(gain_label)
    layout.addLayout(gain_row)

    norm = QCheckBox("Normalize per trace")
    norm.setChecked(True)
    norm.toggled.connect(wf.set_normalize)
    layout.addWidget(norm)
    return widget


def make_record_section_tab():
    n_traces = 16
    n_samples = 4000
    t = np.linspace(0, 20, n_samples).astype(np.float64)
    rng = np.random.default_rng(7)
    distances = np.sort(rng.uniform(0, 100, n_traces))
    y = np.zeros((n_samples, n_traces), dtype=np.float64)
    for i in range(n_traces):
        onset = distances[i] / 5.0
        pulse = np.exp(-((t - onset) / 0.3) ** 2) * np.sin(2 * np.pi * 2 * (t - onset))
        y[:, i] = pulse + 0.05 * rng.standard_normal(n_samples)

    widget = QWidget()
    layout = QVBoxLayout(widget)
    plot = SciQLopPlot()
    layout.addWidget(plot)

    plot.plot(t, y, graph_type=GraphType.Waterfall,
              offsets=distances.tolist(),
              normalize=False, gain=3.0,
              labels=[f"stn{i:02d}" for i in range(n_traces)])
    return widget


def main():
    app = QApplication.instance() or QApplication(sys.argv)
    tabs = QTabWidget()
    tabs.addTab(make_channel_stack_tab(), "Channel stack")
    tabs.addTab(make_record_section_tab(), "Record section")
    tabs.resize(900, 700)
    tabs.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

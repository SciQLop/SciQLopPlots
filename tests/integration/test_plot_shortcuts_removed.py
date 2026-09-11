"""SciQLopPlotInterface used to hard-code QShortcuts for 'M' (autoscale),
'L' (toggle log) and 'H' (toggle visibility). Those are now removed so a host
application (SciQLop) can own and configure the key bindings itself; the
underlying behavior is exposed as plain public methods instead.

This file pins: (1) the new methods work when called directly, (2) the old
key presses no longer trigger anything.
"""
import numpy as np
import pytest
from PySide6.QtCore import Qt, QPoint
from PySide6.QtGui import QCursor

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange


@pytest.fixture
def sample_data():
    x = np.linspace(0, 10, 100).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    return x, y


@pytest.fixture(autouse=True)
def cursor_off_widget():
    """rescale/log-toggle prefer the axis under the cursor over the selected
    one; keep the cursor away from the plot so these tests exercise the
    'selected axes' branch deterministically instead of an incidental hover."""
    QCursor.setPos(QPoint(-10000, -10000))


class TestRescaleHoveredOrSelectedAxes:
    @staticmethod
    def _pump(n=20):
        from PySide6.QtCore import QCoreApplication
        for _ in range(n):
            QCoreApplication.processEvents()

    def test_direct_call_rescales_selected_axis(self, plot, sample_data):
        x, y = sample_data
        plot.line(x, y)
        plot.replot(True)
        self._pump()
        ax = plot.x_axis()
        ax.set_selected(True)
        ax.set_range(SciQLopPlotRange(100.0, 200.0))  # well outside the data
        plot.rescale_hovered_or_selected_axes()
        rng = ax.range()
        assert rng.start() < 1.0 and rng.stop() > 9.0

    def test_key_m_no_longer_rescales(self, qtbot, plot, sample_data):
        x, y = sample_data
        plot.line(x, y)
        plot.replot(True)
        self._pump()
        ax = plot.x_axis()
        ax.set_selected(True)
        ax.set_range(SciQLopPlotRange(100.0, 200.0))
        qtbot.keyClick(plot, Qt.Key_M)
        rng = ax.range()
        assert rng.start() == pytest.approx(100.0)
        assert rng.stop() == pytest.approx(200.0)


class TestToggleLogScaleHoveredOrSelectedAxes:
    def test_direct_call_toggles_log(self, plot):
        ax = plot.y_axis()
        ax.set_selected(True)
        ax.set_log(False)
        plot.toggle_log_scale_hovered_or_selected_axes()
        assert ax.log() is True

    def test_key_l_no_longer_toggles_log(self, qtbot, plot):
        ax = plot.y_axis()
        ax.set_selected(True)
        ax.set_log(False)
        qtbot.keyClick(plot, Qt.Key_L)
        assert ax.log() is False


class TestToggleSelectedObjectsVisibility:
    def test_now_public_and_callable(self, plot):
        plot.toggle_selected_objects_visibility()  # must not raise

    def test_key_h_no_longer_toggles_visibility(self, qtbot, plot, sample_data):
        x, y = sample_data
        graph = plot.line(x, y)
        graph.set_visible(True)
        qtbot.keyClick(plot, Qt.Key_H)
        assert graph.visible() is True

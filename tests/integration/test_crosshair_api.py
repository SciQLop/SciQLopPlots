"""Crosshair: programmatic control from Python and cross-plot cursor sync.

The crosshair could previously only be toggled from Python -- driving it to a
key and reading where it sits lived on the internal `_impl::SciQLopPlot`. That
also meant time-series -> XY (projection) cursor sync had no reachable
mechanism.
"""
import math

import numpy as np
import pytest
from PySide6.QtCore import QEvent, Qt
from PySide6.QtGui import QMouseEvent
from PySide6.QtWidgets import QApplication, QWidget

from SciQLopPlots import (
    GraphType,
    SciQLopMultiPlotPanel,
    SciQLopNDProjectionPlot,
    SciQLopPlot,
    SciQLopTimeSeriesPlot,
)
from conftest import process_events

T0 = 1e9  # a plausible epoch so time-axis plots behave like real ones


@pytest.fixture
def trajectory():
    """(t, x, y, z) for a projection plot -- the N+1 'trajectory' data path."""
    t = np.linspace(T0, T0 + 100, 200)
    a = np.linspace(0, 4 * np.pi, 200)
    return [t, np.cos(a), np.sin(a), a / 10.0]


class TestCrosshairDriveAPI:
    def test_show_crosshair_at_key_records_the_key(self, plot):
        plot.set_crosshair_enabled(True)
        plot.show_crosshair_at_key(4.25)
        assert plot.crosshair_key() == pytest.approx(4.25)

    def test_hide_crosshair_clears_the_key(self, plot):
        plot.set_crosshair_enabled(True)
        plot.show_crosshair_at_key(4.25)
        plot.hide_crosshair()
        assert math.isnan(plot.crosshair_key())

    def test_key_is_nan_before_the_crosshair_is_shown(self, plot):
        plot.set_crosshair_enabled(True)
        assert math.isnan(plot.crosshair_key())

    def test_show_is_ignored_while_the_crosshair_is_disabled(self, plot):
        plot.set_crosshair_enabled(False)
        plot.show_crosshair_at_key(4.25)
        assert math.isnan(plot.crosshair_key())

    def test_disabling_hides_a_shown_crosshair(self, plot):
        plot.set_crosshair_enabled(True)
        plot.show_crosshair_at_key(4.25)
        plot.set_crosshair_enabled(False)
        assert math.isnan(plot.crosshair_key())

    def test_showing_does_not_re_emit_cursor_time_changed(self, plot):
        """Re-emitting would feed the synchronizer back into itself."""
        plot.set_crosshair_enabled(True)
        seen = []
        plot.cursor_time_changed.connect(seen.append)
        plot.show_crosshair_at_key(4.25)
        process_events()
        assert seen == []


class TestCursorSyncBetweenPlots:
    def _panel_with_two_plots(self, qtbot, sample_data):
        panel = SciQLopMultiPlotPanel(None, synchronize_x=False,
                                      synchronize_time=True)
        qtbot.addWidget(panel)
        plots = []
        for _ in range(2):
            p = SciQLopTimeSeriesPlot()
            panel.add_plot(p)
            p.plot(sample_data[0] + T0, sample_data[1])
            p.set_crosshair_enabled(True)
            plots.append(p)
        process_events()
        return panel, plots

    def test_cursor_on_one_plot_moves_the_crosshair_on_its_sibling(
            self, qtbot, sample_data):
        _panel, (src, dst) = self._panel_with_two_plots(qtbot, sample_data)
        src.cursor_time_changed.emit(T0 + 3.0)
        process_events()
        assert dst.crosshair_key() == pytest.approx(T0 + 3.0)

    def test_source_plot_is_not_driven_by_its_own_cursor(
            self, qtbot, sample_data):
        _panel, (src, _dst) = self._panel_with_two_plots(qtbot, sample_data)
        src.cursor_time_changed.emit(T0 + 3.0)
        process_events()
        assert math.isnan(src.crosshair_key())

    def test_nan_hides_the_sibling_crosshair(self, qtbot, sample_data):
        _panel, (src, dst) = self._panel_with_two_plots(qtbot, sample_data)
        src.cursor_time_changed.emit(T0 + 3.0)
        process_events()
        src.cursor_time_changed.emit(float("nan"))
        process_events()
        assert math.isnan(dst.crosshair_key())

    def test_a_removed_plot_is_no_longer_driven(self, qtbot, sample_data):
        panel, (src, dst) = self._panel_with_two_plots(qtbot, sample_data)
        panel.remove_plot(dst)
        process_events()
        src.cursor_time_changed.emit(T0 + 3.0)
        process_events()
        assert math.isnan(dst.crosshair_key())


class TestTimeSeriesToProjectionSync:
    def _panel(self, qtbot, trajectory, sample_data):
        panel = SciQLopMultiPlotPanel(None, synchronize_x=False,
                                      synchronize_time=True)
        qtbot.addWidget(panel)
        ts = SciQLopTimeSeriesPlot()
        panel.add_plot(ts)
        ts.plot(sample_data[0] + T0, sample_data[1])
        ts.set_crosshair_enabled(True)
        proj = SciQLopNDProjectionPlot(3)
        proj.plot(trajectory, graph_type=GraphType.ParametricCurve)
        panel.add_plot(proj)
        process_events()
        return panel, ts, proj

    def test_time_marker_key_is_nan_by_default(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        assert math.isnan(proj.time_marker_key())

    def test_set_time_marker_records_the_key(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_time_marker(T0 + 10)
        assert proj.time_marker_key() == pytest.approx(T0 + 10)

    def test_clear_time_marker_resets_the_key(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_time_marker(T0 + 10)
        proj.clear_time_marker()
        assert math.isnan(proj.time_marker_key())

    def test_time_series_cursor_drives_the_projection_time_marker(
            self, qtbot, trajectory, sample_data):
        _panel, ts, proj = self._panel(qtbot, trajectory, sample_data)
        ts.cursor_time_changed.emit(T0 + 42.0)
        process_events()
        assert proj.time_marker_key() == pytest.approx(T0 + 42.0)

    def test_nan_clears_the_projection_time_marker(
            self, qtbot, trajectory, sample_data):
        _panel, ts, proj = self._panel(qtbot, trajectory, sample_data)
        ts.cursor_time_changed.emit(T0 + 42.0)
        process_events()
        ts.cursor_time_changed.emit(float("nan"))
        process_events()
        assert math.isnan(proj.time_marker_key())

    def test_projection_subplots_are_not_driven_as_crosshairs(
            self, qtbot, trajectory, sample_data):
        """A projection's XY subplots have no time axis -- only the marker moves."""
        _panel, ts, proj = self._panel(qtbot, trajectory, sample_data)
        ts.cursor_time_changed.emit(T0 + 42.0)
        process_events()
        assert all(math.isnan(proj.subplot(i).crosshair_key())
                   for i in range(proj.subplot_count()))


class TestCursorSyncEndToEnd:
    """Proves the whole chain: mouse move -> hover key -> sibling crosshair."""

    def test_a_mouse_move_propagates_to_the_sibling_plot(
            self, qtbot, sample_data):
        panel = SciQLopMultiPlotPanel(None, synchronize_x=False,
                                      synchronize_time=True)
        qtbot.addWidget(panel)
        src, dst = SciQLopTimeSeriesPlot(), SciQLopTimeSeriesPlot()
        for p in (src, dst):
            panel.add_plot(p)
            p.plot(sample_data[0] + T0, sample_data[1])
            p.set_crosshair_enabled(True)
        panel.resize(600, 600)
        panel.show()
        qtbot.waitExposed(panel)
        process_events()

        # The mouse handler lives on the inner _impl::SciQLopPlot widget, not on
        # the SciQLopPlot frame, so the move has to target that child. It is
        # posted directly rather than through qtbot.mouseMove, which needs the
        # window manager to actually warp the pointer.
        qcp = src.findChild(QWidget)
        assert qcp is not None
        centre = qcp.rect().center()
        QApplication.sendEvent(qcp, QMouseEvent(
            QEvent.MouseMove, centre.toPointF(),
            qcp.mapToGlobal(centre).toPointF(),
            Qt.NoButton, Qt.NoButton, Qt.NoModifier))
        process_events()

        assert not math.isnan(src.crosshair_key()), "source crosshair did not move"
        assert dst.crosshair_key() == pytest.approx(src.crosshair_key())

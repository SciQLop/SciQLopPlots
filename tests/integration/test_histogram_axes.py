"""Histogram axes: a 2-D histogram callback follows time, not its value axes; a panel
does not drag a histogram's value x axis; re-binning follows set_x_axis/set_y_axis."""
import numpy as np

from SciQLopPlots import AxisType, SciQLopMultiPlotPanel


def _recording_scatter(fetched):
    def cb(start, stop):
        fetched.append((start, stop))
        x = np.linspace(start, stop, 100)
        return x, np.sin(x)
    return cb


class TestHistogram2DCallable:
    def test_fetches_the_time_window_not_the_value_axis(self, qtbot, plot):
        fetched = []
        plot.x_axis().set_range(-3.0, 3.0)
        plot.histogram2d(_recording_scatter(fetched))
        plot.x_axis().set_range(-4.0, 4.0)
        plot.time_axis().set_range(100.0, 200.0)
        qtbot.waitUntil(lambda: (100.0, 200.0) in fetched, timeout=5000)
        assert (-3.0, 3.0) not in fetched
        assert (-4.0, 4.0) not in fetched

    def test_panel_callable_starts_on_the_panel_time_range(self, qtbot):
        panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
        qtbot.addWidget(panel)
        panel.set_time_axis_range(50.0, 60.0)
        fetched = []
        panel.histogram2d(_recording_scatter(fetched))
        qtbot.waitUntil(lambda: (50.0, 60.0) in fetched, timeout=5000)


class TestPanelHistogram2DAxes:
    def _line_and_histogram(self, panel):
        line_plot, _ = panel.plot(np.linspace(0, 100, 1000), np.cos(np.linspace(0, 100, 1000)))
        rng = np.random.default_rng(0)
        hplot, _ = panel.histogram2d(rng.normal(500.0, 1.0, 1000), rng.normal(size=1000))
        return line_plot, hplot

    def test_shared_x_does_not_drag_the_histogram(self, panel):
        line_plot, hplot = self._line_and_histogram(panel)
        before = hplot.x_axis().range()
        line_plot.x_axis().set_range(10.0, 20.0)
        assert hplot.x_axis().range() == before

    def test_histogram_does_not_drag_the_shared_x(self, panel):
        line_plot, hplot = self._line_and_histogram(panel)
        hplot.x_axis().set_range(495.0, 505.0)
        assert line_plot.x_axis().range().stop() <= 101.0

    def test_histogram_keeps_its_x_axis_visible(self, panel):
        hplot, _ = panel.histogram2d(np.arange(10.0), np.arange(10.0))
        panel.plot(np.arange(10.0), np.arange(10.0))
        assert hplot.x_axis().visible()


def _log_spaced(edges):
    edges = np.asarray(edges)
    return np.allclose(np.diff(np.log(edges)), np.log(edges[1] / edges[0]))


class TestRebinOnTheNewAxis:
    def test_histogram_rebins_when_its_new_x_axis_turns_log(self, qtbot, plot):
        h = plot.histogram(np.geomspace(1.0, 1000.0, 500), bins=10)
        qtbot.waitUntil(lambda: len(h.bin_edges()) == 11, timeout=5000)
        h.set_x_axis(plot.x2_axis())
        plot.x2_axis().set_log(True)
        qtbot.waitUntil(lambda: _log_spaced(h.bin_edges()), timeout=5000)

    def test_histogram_ignores_its_old_x_axis(self, qtbot, plot):
        h = plot.histogram(np.geomspace(1.0, 1000.0, 500), bins=10)
        qtbot.waitUntil(lambda: len(h.bin_edges()) == 11, timeout=5000)
        h.set_x_axis(plot.x2_axis())
        plot.x_axis().set_log(True)
        qtbot.wait(200)
        assert not _log_spaced(h.bin_edges())

    def test_histogram2d_rebins_when_its_new_axes_turn_log(self, qtbot, plot):
        h = plot.histogram2d(np.geomspace(1.0, 1000.0, 500), np.geomspace(1.0, 1000.0, 500))
        h.set_x_axis(plot.x2_axis())
        h.set_y_axis(plot.y2_axis())
        with qtbot.waitSignal(h.x_bins_log_changed, timeout=1000):
            plot.x2_axis().set_log(True)
        with qtbot.waitSignal(h.y_bins_log_changed, timeout=1000):
            plot.y2_axis().set_log(True)
        assert h.x_bins_log() and h.y_bins_log()

    def test_histogram2d_ignores_its_old_axes(self, qtbot, plot):
        h = plot.histogram2d(np.geomspace(1.0, 1000.0, 500), np.geomspace(1.0, 1000.0, 500))
        h.set_x_axis(plot.x2_axis())
        changes = []
        h.x_bins_log_changed.connect(changes.append)
        plot.x_axis().set_log(True)
        assert changes == []

    def test_histogram_keeps_its_auto_edges_on_a_same_scale_axis(self, qtbot, plot):
        h = plot.histogram(np.linspace(0.0, 100.0, 500), bins=10)
        qtbot.waitUntil(lambda: len(h.bin_edges()) == 11, timeout=5000)
        h.set_values(np.linspace(40.0, 60.0, 500))
        qtbot.wait(200)
        assert h.bin_edges()[0] == 0.0
        h.set_x_axis(plot.x2_axis())
        qtbot.wait(200)
        assert (h.bin_edges()[0], h.bin_edges()[-1]) == (0.0, 100.0)

    def test_histogram2d_does_not_rebin_on_a_same_scale_axis(self, qtbot, plot):
        h = plot.histogram2d(np.linspace(0.0, 1.0, 500), np.linspace(0.0, 1.0, 500))
        qtbot.wait(200)
        busy = []
        h.busy_changed.connect(busy.append)
        h.set_x_axis(plot.x2_axis())
        h.set_y_axis(plot.y2_axis())
        qtbot.wait(200)
        assert busy == []


class TestLateAxisSynchronization:
    def _two_plots(self, panel):
        p1, _ = panel.plot(np.arange(10.0), np.arange(10.0))
        p2, _ = panel.plot(np.arange(10.0), np.arange(10.0))
        return p1, p2

    def test_opting_out_after_adding_stops_the_drag(self, panel):
        p1, p2 = self._two_plots(panel)
        p2.set_axis_synchronized(AxisType.XAxis, False)
        p1.x_axis().set_range(10.0, 20.0)
        assert p2.x_axis().range().start() != 10.0

    def test_opted_out_plot_does_not_drag_the_panel(self, panel):
        p1, p2 = self._two_plots(panel)
        p2.set_axis_synchronized(AxisType.XAxis, False)
        p2.x_axis().set_range(10.0, 20.0)
        assert p1.x_axis().range().start() != 10.0

    def test_opting_back_in_restores_the_drag(self, panel):
        p1, p2 = self._two_plots(panel)
        p2.set_axis_synchronized(AxisType.XAxis, False)
        p2.set_axis_synchronized(AxisType.XAxis, True)
        p1.x_axis().set_range(30.0, 40.0)
        assert (p2.x_axis().range().start(), p2.x_axis().range().stop()) == (30.0, 40.0)

    def test_opted_out_plot_shows_its_x_axis(self, panel):
        p1, p2 = self._two_plots(panel)
        assert not p1.x_axis().visible()
        p1.set_axis_synchronized(AxisType.XAxis, False)
        assert p1.x_axis().visible() and p2.x_axis().visible()

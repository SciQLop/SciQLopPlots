"""1-D histogram (docs/plans/2026-10-21-bands-errorbars-histogram-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import (GraphType, HistogramNormalization, HistogramStyle, SciQLopHistogram,
                          SciQLopMultiPlotPanel, SciQLopPlot)


class TestHistogramClass:
    def test_bins_must_be_positive(self, plot):
        h = plot.histogram(np.random.default_rng(0).normal(size=100))
        with pytest.raises(ValueError):
            h.set_bins(0)

    def test_bad_range_raises(self, plot):
        h = plot.histogram(np.arange(10.0))
        with pytest.raises(ValueError):
            h.set_bin_range(5.0, 1.0)
        h.set_log_bins(True)
        with pytest.raises(ValueError):
            h.set_bin_range(0.0, 10.0)

    def test_log_bins_set_the_x_axis_to_log(self, plot):
        h = plot.histogram(np.arange(1.0, 100.0))
        h.set_log_bins(True)
        assert plot.x_axis().log()

    def test_integer_values_are_binned(self, qtbot, plot):
        h = plot.histogram(np.array([1, 1, 2, 3], dtype=np.int32))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 1, timeout=5000)
        assert sum(h.bin_heights()[0]) == 4


def _wait_result(qtbot, h):
    qtbot.waitUntil(lambda: len(h.bin_heights()) > 0, timeout=5000)


class TestHistogramBehaviour:
    def test_factory_returns_a_histogram(self, plot):
        assert isinstance(plot.histogram(np.arange(10.0)), SciQLopHistogram)

    def test_edges_have_one_more_entry_than_heights(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=10)
        _wait_result(qtbot, h)
        assert len(h.bin_edges()) == 11
        assert len(h.bin_heights()[0]) == 10

    def test_one_column_defaults_to_bars(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        _wait_result(qtbot, h)
        assert h.style() == HistogramStyle.Bars

    def test_several_columns_default_to_step(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        h.set_data(np.arange(10.0), np.random.default_rng(0).normal(size=(10, 2)))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 2, timeout=5000)
        assert h.style() == HistogramStyle.Step

    def test_explicit_style_wins(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        h.set_style(HistogramStyle.Bars)
        h.set_data(np.arange(10.0), np.random.default_rng(0).normal(size=(10, 2)))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 2, timeout=5000)
        assert h.style() == HistogramStyle.Bars

    def test_column_major_columns_are_binned_apart(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        y = np.asfortranarray(np.column_stack([np.zeros(10), np.ones(10) * 5.0]))
        h.set_data(np.arange(10.0), y)
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 2, timeout=5000)
        assert [sum(c) for c in h.bin_heights()] == [10, 10]
        assert h.bin_heights()[0][0] == 10

    def test_binned_bars(self, plot):
        h = plot.histogram(np.arange(10.0))
        h.set_binned(np.array([0.0, 1.0, 2.0]), np.array([3.0, 4.0]))
        assert h.bin_edges() == [0.0, 1.0, 2.0]
        assert h.bin_heights() == [[3.0, 4.0]]

    def test_binned_size_mismatch_raises(self, plot):
        h = plot.histogram(np.arange(10.0))
        with pytest.raises(ValueError):
            h.set_binned(np.array([0.0, 1.0, 2.0]), np.array([3.0, 4.0, 5.0]))

    def test_probability_sums_to_one(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=8)
        _wait_result(qtbot, h)
        h.set_normalization(HistogramNormalization.Probability)
        qtbot.waitUntil(lambda: abs(sum(h.bin_heights()[0]) - 1.0) < 1e-9, timeout=5000)

    def test_fixed_range_sets_the_edges(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=4)
        h.set_bin_range(0.0, 200.0)
        qtbot.waitUntil(lambda: h.bin_edges()[-1:] == [200.0], timeout=5000)
        assert h.bin_edges() == [0.0, 50.0, 100.0, 150.0, 200.0]

    def test_bins_changed_when_edges_move(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=4)
        _wait_result(qtbot, h)
        with qtbot.waitSignal(h.bins_changed, timeout=5000):
            h.set_bins(8)

    def test_first_result_fits_the_x_axis(self, qtbot, plot):
        h = plot.histogram(np.arange(1000.0, 2000.0))
        _wait_result(qtbot, h)
        qtbot.waitUntil(lambda: plot.x_axis().range().start() >= 900.0, timeout=5000)
        r = plot.x_axis().range()
        assert r.start() <= h.bin_edges()[0] and r.stop() >= h.bin_edges()[-1]

    def test_a_histogram_can_leave_the_legend(self, plot):
        h = plot.histogram(np.arange(10.0), name="counts")
        assert plot.legend().row_labels() == ["counts"]
        h.set_in_legend(False)
        assert plot.legend().row_labels() == []

    def test_infinite_range_raises(self, plot):
        h = plot.histogram(np.arange(10.0))
        with pytest.raises(ValueError):
            h.set_bin_range(0.0, np.inf)
        with pytest.raises(ValueError):
            h.set_bin_range(-np.inf, 1.0)


class TestFactories:
    def test_plot_dispatch_one_argument(self, qtbot, plot):
        h = plot.plot(np.arange(10.0), graph_type=GraphType.Histogram, name="h", bins=5)
        assert isinstance(h, SciQLopHistogram) and h.bins() == 5 and h.name == "h"

    def test_two_arguments_are_refused(self, plot):
        with pytest.raises(TypeError):
            plot.plot(np.arange(10.0), np.arange(10.0), graph_type=GraphType.Histogram)

    def test_histogram_kwargs_on_a_line_are_refused(self, plot):
        with pytest.raises(TypeError):
            plot.plot(np.arange(10.0), np.arange(10.0), bins=32)

    def test_histogram_kwargs_on_a_line_callback_are_refused(self, plot):
        with pytest.raises(TypeError, match="Histogram"):
            plot.plot(lambda start, stop: None, graph_type=GraphType.Line, log_bins=True)

    def test_bars_from_edges_and_heights(self, qtbot, plot):
        b = plot.bars(np.array([0.0, 1.0, 2.0]), np.array([3.0, 5.0]))
        assert list(b.bin_heights()[0]) == [3.0, 5.0]
        with pytest.raises(ValueError):
            plot.bars(np.array([0.0, 1.0]), np.array([3.0, 5.0]))

    def test_bars_report_their_bar_count_as_bins(self, plot):
        b = plot.bars(np.array([0.0, 1.0, 2.0, 5.0]), np.array([3.0, 5.0, 1.0]))
        assert b.bins() == 3
        assert list(b.bin_heights()[0]) == [3.0, 5.0, 1.0]

    def test_refused_bars_leave_nothing_on_the_plot(self, plot):
        with pytest.raises(ValueError):
            plot.bars(np.array([0.0, 1.0]), np.array([3.0, 5.0]))
        assert plot.legend().row_labels() == []

    def test_range_excludes_outside_values(self, qtbot, plot):
        h = plot.histogram(np.array([-10.0, 0.5, 1.5, 10.0]), bins=2, range=(0.0, 2.0))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 1, timeout=5000)
        assert sum(h.bin_heights()[0]) == 2

    def test_python_kwargs_configure_the_histogram(self, qtbot, plot):
        h = plot.histogram(np.arange(1.0, 100.0), log_bins=True,
                           normalization=HistogramNormalization.Density,
                           style=HistogramStyle.Step)
        assert h.log_bins() and plot.x_axis().log()
        assert h.normalization() == HistogramNormalization.Density
        assert h.style() == HistogramStyle.Step

    def test_panel_histogram_follows_the_time_range(self, qtbot, panel):
        fetched = []

        def cb(start, stop):
            fetched.append((start, stop))
            x = np.linspace(start, stop, 100)
            return x, np.sin(x)

        panel.plot(np.linspace(0, 100, 1000), np.cos(np.linspace(0, 100, 1000)))
        hplot, h = panel.histogram(cb)
        panel.set_time_axis_range(10.0, 20.0)
        qtbot.waitUntil(lambda: any(abs(a - 10.0) < 1e-9 for a, _ in fetched), timeout=5000)
        x_range = hplot.x_axis().range()
        assert not (abs(x_range.start() - 10.0) < 1e-9 and abs(x_range.stop() - 20.0) < 1e-9)

    def test_empty_fetch_then_data(self, qtbot, plot):
        calls = []

        def cb(start, stop):
            calls.append(1)
            if len(calls) == 1:
                return np.array([], dtype=np.float64), np.array([], dtype=np.float64)
            return np.linspace(start, stop, 10), np.arange(10.0)

        h = plot.histogram(cb)
        plot.time_axis().set_range(0.0, 1.0)
        # Back to back, the pipeline would keep only the last window: one call, empty.
        qtbot.waitUntil(lambda: len(calls) == 1, timeout=5000)
        plot.time_axis().set_range(0.0, 2.0)
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 1, timeout=5000)

    def test_callable_fetches_the_time_window_not_the_value_axis(self, qtbot, plot):
        fetched = []

        def cb(start, stop):
            fetched.append((start, stop))
            return np.linspace(start, stop, 10), np.arange(10.0)

        plot.histogram(cb)
        plot.x_axis().set_range(-3.0, 3.0)
        plot.time_axis().set_range(100.0, 200.0)
        qtbot.waitUntil(lambda: (100.0, 200.0) in fetched, timeout=5000)
        assert (-3.0, 3.0) not in fetched

    def test_panel_callable_starts_on_the_panel_time_range(self, qtbot):
        panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
        qtbot.addWidget(panel)
        panel.set_time_axis_range(50.0, 60.0)
        fetched = []

        def cb(start, stop):
            fetched.append((start, stop))
            return np.linspace(start, stop, 10), np.arange(10.0)

        panel.histogram(cb)
        qtbot.waitUntil(lambda: (50.0, 60.0) in fetched, timeout=5000)


class TestPanelAxes:
    def _line_and_histogram(self, panel):
        line_plot, _ = panel.plot(np.linspace(0, 100, 1000), np.cos(np.linspace(0, 100, 1000)))
        hplot, h = panel.histogram(np.random.default_rng(0).normal(500.0, 1.0, 1000))
        return line_plot, hplot, h

    def test_histogram_does_not_drag_the_shared_x(self, qtbot, panel):
        line_plot, hplot, h = self._line_and_histogram(panel)
        _wait_result(qtbot, h)
        qtbot.waitUntil(lambda: hplot.x_axis().range().start() > 400.0, timeout=5000)
        assert line_plot.x_axis().range().stop() <= 101.0

    def test_shared_x_does_not_drag_the_histogram(self, qtbot, panel):
        line_plot, hplot, h = self._line_and_histogram(panel)
        _wait_result(qtbot, h)
        qtbot.waitUntil(lambda: hplot.x_axis().range().start() > 400.0, timeout=5000)
        line_plot.x_axis().set_range(10.0, 20.0)
        assert hplot.x_axis().range().start() > 400.0

    def test_histogram_keeps_its_x_axis_visible(self, qtbot, panel):
        hplot, _ = panel.histogram(np.arange(10.0))
        panel.plot(np.arange(10.0), np.arange(10.0))
        assert hplot.x_axis().visible()


class TestAutoScale:
    def test_auto_scale_follows_moved_edges(self, qtbot, plot):
        plot.set_fit_first_data(False)
        plot.set_auto_scale(True)
        h = plot.histogram(np.arange(100.0), bins=4)
        _wait_result(qtbot, h)
        h.set_bin_range(0.0, 1000.0)
        qtbot.waitUntil(lambda: plot.x_axis().range().stop() >= 1000.0, timeout=5000)

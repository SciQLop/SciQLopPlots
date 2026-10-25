"""Phase 1 of the 2026-10-20 API review (docs/api-review-2026-10-20.md): hygiene, no behaviour
change. Old names stay as aliases; only what Python could reach by accident goes away."""
from datetime import datetime, timezone

import numpy as np
import pytest

import SciQLopPlots
from SciQLopPlots import (LineTermination, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopPlotRange, SciQLopTimeSeriesPlot, SciQLopVerticalSpan)

X = np.linspace(0, 1, 10)


class TestStarImport:  # task 1, finding 3.4
    def _star(self):
        namespace = {}
        exec("from SciQLopPlots import *", namespace)
        return namespace

    @pytest.mark.parametrize("leak", ["np", "sys", "functools", "traceback", "datetime", "date",
                                      "timezone", "QtCore", "QtGui", "QtWidgets", "QtSvg"])
    def test_does_not_leak_imports(self, leak):
        assert leak not in self._star()

    @pytest.mark.parametrize("name", ["SciQLopPlot", "SciQLopMultiPlotPanel", "SciQLopPlotRange",
                                      "GraphType", "WidgetPlot", "Pipeline", "Event", "tracing",
                                      "SciQLopPlotGroup", "SciQLopNDProjectionPlot"])
    def test_brings_the_api(self, name):
        assert name in self._star()

    def test_every_listed_name_exists(self):
        assert [n for n in SciQLopPlots.__all__ if not hasattr(SciQLopPlots, n)] == []


class TestInternalsHidden:  # task 2, findings 3.3, 3.4
    @pytest.mark.parametrize("cls, field", [
        (SciQLopPlot, "m_legend"), (SciQLopPlot, "m_overlay"), (SciQLopPlot, "m_auto_scale"),
        (SciQLopNDProjectionPlot, "m_plots"), (SciQLopNDProjectionPlot, "m_pane_names"),
        (SciQLopPlots.SciQLopPlottableInterface, "m_metaData"),
        (SciQLopPlots.SciQLopPlottableInterface, "m_in_legend"),
    ])
    def test_protected_fields_are_not_reachable(self, cls, field):
        assert not hasattr(cls, field)

    def test_no_class_exposes_a_protected_field(self):
        leaks = [f"{n}.{m}" for n in dir(SciQLopPlots)
                 if isinstance(getattr(SciQLopPlots, n), type)
                 for m in vars(getattr(SciQLopPlots, n)) if m.startswith("m_")]
        assert leaks == []

    @pytest.mark.parametrize("cls, method", [
        (SciQLopPlots.SciQLopNDProjectionCurves, "check_batch_size"),
        (SciQLopPlots.SciQLopNDProjectionCurves, "is_pane_curve"),
        (SciQLopPlots.SciQLopMultiGraphBase, "sync_components"),
        (SciQLopPlots.SciQLopMultiGraphBase, "build_data_source"),
        (SciQLopPlots.SciQLopMultiGraphBase, "create_graphs"),
        (SciQLopPlots.SciQLopMultiGraphBase, "clear_graphs"),
        (SciQLopPlots.SciQLopPlottableInterface, "check_first_data"),
        (SciQLopPlots.SciQLopPlottableInterface, "data_off_view"),
    ])
    def test_plumbing_methods_are_not_reachable(self, cls, method):
        assert not hasattr(cls, method)

    def test_plot_impl_stays(self):
        # SciQLop uses it (23 call sites).
        assert hasattr(SciQLopPlot, "plot_impl")


class TestSnakeCaseAliases:  # task 3, findings 1.5, 1.6
    @pytest.mark.parametrize("cls, new, old", [
        (SciQLopMultiPlotPanel, "set_selected", "setSelected"),
        (SciQLopMultiPlotPanel, "add_widget", "addWidget"),
        (SciQLopMultiPlotPanel, "remove_widget", "removeWidget"),
        (SciQLopMultiPlotPanel, "index_of", "indexOf"),
        (SciQLopPlots.MultiPlotsVerticalLine, "add_object", "addObject"),
        (SciQLopPlots.MultiPlotsVerticalLine, "remove_object", "removeObject"),
    ])
    def test_methods_have_both_names(self, cls, new, old):
        assert getattr(cls, new) is getattr(cls, old)

    def test_panel_set_selected_works(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        panel.set_selected(True)
        assert panel.selected()

    @pytest.mark.parametrize("cls", [SciQLopMultiPlotPanel, SciQLopVerticalSpan,
                                     SciQLopPlots.SciQLopHorizontalSpan,
                                     SciQLopPlots.SciQLopRectangularSpan])
    def test_selection_signal_has_both_names(self, cls):
        assert hasattr(cls, "selection_changed") and hasattr(cls, "selectionChanged")

    def test_span_selection_changed_fires(self, qtbot, plot):
        span = plot.vertical_span(SciQLopPlotRange(0.2, 0.4))
        seen = []
        span.selection_changed.connect(seen.append)
        span.set_selected(True)
        assert seen == [True]

    def test_panel_selection_changed_fires(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        seen = []
        panel.selection_changed.connect(seen.append)
        panel.set_selected(True)
        assert seen == [True]


class TestMetadataKeyword:  # task 4, finding 1.4
    def test_metadata_is_accepted(self, plot):
        graph = plot.line(X, X, metadata={"source": "mag"})
        assert graph.meta_data()["source"] == "mag"

    def test_metaData_is_gone_and_the_error_says_why(self, plot):
        # Renamed outright: a Python shim would hide shiboken's signature listing on every bad
        # call. Shiboken reports an unknown keyword as an AttributeError.
        with pytest.raises(AttributeError, match="unsupported keyword 'metaData'"):
            plot.line(X, X, metaData={"source": "mag"})

    def test_metadata_through_plot(self, plot):
        graph = plot.plot(X, X, metadata={"source": "mag"})
        assert graph.meta_data()["source"] == "mag"

    def test_metadata_on_a_panel(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        _, graph = panel.line(X, X, metadata={"source": "mag"})
        assert graph.meta_data()["source"] == "mag"


class TestTimeRanges:  # task 5, finding 3.5
    T0 = datetime(2020, 1, 1, 12, 0, 0, 250000, tzinfo=timezone.utc)
    T1 = datetime(2020, 1, 2, tzinfo=timezone.utc)

    def test_dates_keep_sub_seconds(self):
        r = SciQLopPlotRange(self.T0, self.T1)
        assert r.start_date().toMSecsSinceEpoch() == int(self.T0.timestamp() * 1000)

    def test_date_strings_keep_milliseconds(self):
        r = SciQLopPlotRange("2020-01-01T00:00:00.250", "2020-01-02")
        assert r.start() == pytest.approx(1577836800.25)

    def test_dates_are_utc(self):
        from PySide6.QtCore import QTimeZone
        r = SciQLopPlotRange(self.T0, self.T1)
        assert r.start_date().timeZone() == QTimeZone.utc()
        assert r.start_date().time().hour() == 12

    def test_a_graph_keeps_a_time_range(self, ts_plot):
        # The graph stores its range by assignment, which dropped the time flag.
        graph = ts_plot.plot(lambda start, stop: None)
        graph.set_range(SciQLopPlotRange(self.T0, self.T1))
        assert repr(graph.range()).startswith("SciQLopPlotTimeRange")

    def test_a_multi_plot_span_keeps_a_time_range(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        panel.create_plot(-1)
        span = panel.vertical_span(SciQLopPlotRange(self.T0, self.T1))
        assert repr(span.range()).startswith("SciQLopPlotTimeRange")


class TestLeftovers:  # task 6, finding 3.10
    def test_unity_is_gone(self):
        assert not hasattr(SciQLopTimeSeriesPlot, "unity")

    def test_spike_arrow_is_spelled_right(self):
        assert LineTermination.SpikeArrow == LineTermination.SPikeArrow

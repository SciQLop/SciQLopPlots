"""Inspector row order must mirror the panel's splitter order when plots/panels
are inserted at non-end positions via insert_plot / create_plot(index=...) /
insert_panel.

Regression: PlotsModel::addNode always appended to the end of children,
so insert_plot(0, plot) made the inspector show the new plot at the bottom
while the panel UI placed it at the top.
"""
import pytest

from PySide6.QtCore import QModelIndex

from SciQLopPlots import (
    PlotsModel,
    PlotType,
    SciQLopMultiPlotPanel,
    SciQLopPlot,
    SciQLopTimeSeriesPlot,
)
from conftest import process_events


def _panel_index(model, panel):
    for row in range(model.rowCount(QModelIndex())):
        idx = model.index(row, 0, QModelIndex())
        if PlotsModel.object(idx) is panel:
            return idx
    raise AssertionError("panel not found in model")


def _plot_row_in_model(model, panel_idx, plot):
    for row in range(model.rowCount(panel_idx)):
        idx = model.index(row, 0, panel_idx)
        if PlotsModel.object(idx) is plot:
            return row
    return -1


def _model_plot_order(model, panel_idx, plots):
    return [_plot_row_in_model(model, panel_idx, p) for p in plots]


def _panel_plot_order(panel, plots):
    return [panel.index(p) for p in plots]


class TestInsertPlotOrder:
    def test_insert_at_zero_matches_panel(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        p0 = SciQLopPlot()
        p1 = SciQLopPlot()
        p2 = SciQLopPlot()
        panel.add_plot(p0)
        panel.add_plot(p1)
        panel.insert_plot(0, p2)
        process_events()

        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)

        assert _panel_plot_order(panel, [p0, p1, p2]) == [1, 2, 0]
        assert _model_plot_order(model, panel_idx, [p0, p1, p2]) == [1, 2, 0]

    def test_insert_in_middle_matches_panel(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        p0 = SciQLopPlot()
        p1 = SciQLopPlot()
        p2 = SciQLopPlot()
        panel.add_plot(p0)
        panel.add_plot(p1)
        panel.insert_plot(1, p2)
        process_events()

        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)

        assert _panel_plot_order(panel, [p0, p1, p2]) == [0, 2, 1]
        assert _model_plot_order(model, panel_idx, [p0, p1, p2]) == [0, 2, 1]

    def test_create_plot_with_index_matches_panel(self, qtbot):
        panel = SciQLopMultiPlotPanel(synchronize_time=True)
        qtbot.addWidget(panel)
        p0 = panel.create_plot(plot_type=PlotType.TimeSeries)
        p1 = panel.create_plot(plot_type=PlotType.TimeSeries)
        p_top = panel.create_plot(index=0, plot_type=PlotType.TimeSeries)
        process_events()

        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)
        plots = [p0, p1, p_top]
        assert _panel_plot_order(panel, plots) == [1, 2, 0]
        assert _model_plot_order(model, panel_idx, plots) == [1, 2, 0]

    def test_append_still_works(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        p0 = SciQLopPlot()
        p1 = SciQLopPlot()
        panel.add_plot(p0)
        panel.add_plot(p1)
        process_events()

        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)
        assert _model_plot_order(model, panel_idx, [p0, p1]) == [0, 1]


class TestInsertPanelOrder:
    def test_insert_panel_at_zero_matches_splitter(self, qtbot):
        parent = SciQLopMultiPlotPanel()
        qtbot.addWidget(parent)
        plot0 = SciQLopPlot()
        sub = SciQLopMultiPlotPanel(parent=parent)
        parent.add_plot(plot0)
        parent.insert_panel(0, sub)
        process_events()

        # In the splitter, sub is at widget index 0, plot0 at index 1
        widgets = parent.child_widgets()
        assert widgets[0] is sub
        assert widgets[1] is plot0

        model = PlotsModel.instance()
        parent_idx = _panel_index(model, parent)
        # Inspector children should match splitter order
        row_sub = -1
        row_plot = -1
        for row in range(model.rowCount(parent_idx)):
            idx = model.index(row, 0, parent_idx)
            obj = PlotsModel.object(idx)
            if obj is sub:
                row_sub = row
            elif obj is plot0:
                row_plot = row
        assert row_sub == 0
        assert row_plot == 1

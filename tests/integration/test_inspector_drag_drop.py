"""Drag-and-drop reorder of plots within a panel via PlotsModel.

Drives PlotsModel.mimeData() and dropMimeData() directly — no fake Qt drag event needed.
Verifies (a) panel splitter order updates, (b) PlotsModel row order tracks the panel.
"""
import numpy as np
import pytest

from PySide6.QtCore import QItemSelectionModel, QModelIndex, Qt

from SciQLopPlots import SciQLopMultiPlotPanel, PlotsModel
from conftest import process_events


@pytest.fixture
def panel_with_three_plots(qtbot, sample_data):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    x, y = sample_data
    plot0, _ = panel.line(x, y)
    plot1, _ = panel.line(x, y)
    plot2, _ = panel.line(x, y)
    process_events()
    return panel, [plot0, plot1, plot2]


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


def _drop(model, panel_idx, source_plot, dest_row):
    source_idx = model.index(_plot_row_in_model(model, panel_idx, source_plot), 0, panel_idx)
    mime = model.mimeData([source_idx])
    assert mime.hasFormat("application/x-sciqlop-plot-reorder")
    return model.dropMimeData(mime, Qt.MoveAction, dest_row, 0, panel_idx)


def _panel_order(panel, plots):
    return [panel.index(p) for p in plots]


def _model_order(model, panel_idx, plots):
    return [_plot_row_in_model(model, panel_idx, p) for p in plots]


class TestPlotReorderDragDrop:
    def test_move_first_plot_to_last(self, panel_with_three_plots):
        panel, plots = panel_with_three_plots
        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)

        # Drop plots[0] past the end (dest_row = 3 means "after the last")
        assert _drop(model, panel_idx, plots[0], 3)
        process_events()

        # plots[0] now last, others shifted up
        assert _panel_order(panel, plots) == [2, 0, 1]
        assert _model_order(model, panel_idx, plots) == [2, 0, 1]

    def test_move_last_plot_to_first(self, panel_with_three_plots):
        panel, plots = panel_with_three_plots
        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)

        assert _drop(model, panel_idx, plots[2], 0)
        process_events()

        assert _panel_order(panel, plots) == [1, 2, 0]
        assert _model_order(model, panel_idx, plots) == [1, 2, 0]

    def test_drop_on_same_position_is_noop(self, panel_with_three_plots):
        panel, plots = panel_with_three_plots
        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)

        # plots[1] is at row 1; dropping at insertion-row 1 or 2 means "stay"
        assert not _drop(model, panel_idx, plots[1], 1)
        assert not _drop(model, panel_idx, plots[1], 2)
        assert _panel_order(panel, plots) == [0, 1, 2]

    def test_panel_node_accepts_drops(self, panel_with_three_plots):
        panel, plots = panel_with_three_plots
        model = PlotsModel.instance()
        panel_idx = _panel_index(model, panel)
        plot_idx = model.index(0, 0, panel_idx)

        assert model.flags(panel_idx) & Qt.ItemIsDropEnabled
        assert model.flags(plot_idx) & Qt.ItemIsDragEnabled
        # Plots themselves must not advertise as drop targets — otherwise the
        # OnItem indicator hijacks edge drops and rejects them.
        assert not (model.flags(plot_idx) & Qt.ItemIsDropEnabled)


# NOTE: the view-level fix (PlotsTreeView::dropEvent neutralising MoveAction
# so QAbstractItemView does not auto-removeRows the source) is not unit-tested
# here — Qt's drag-drop event routing requires an internal-state sequence that
# is hard to synthesise reliably from Python. The fix is small and verified
# by manual interaction; if it regresses, plots will visibly disappear on
# drop, which is loud and immediate.

"""Snapshot behavior of SciQLopLineGraph to gate the MultiGraphBase refactor."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot, SciQLopLineGraph, GraphType


@pytest.fixture(scope="module")
def app():
    inst = QApplication.instance()
    return inst if inst else QApplication([])


@pytest.fixture()
def plot(app):
    p = SciQLopPlot()
    yield p
    del p


def test_line_graph_2d_creates_component_per_column(plot):
    x = np.linspace(0, 1, 100).astype(np.float64)
    y = np.column_stack([np.sin(x), np.cos(x), np.sin(2 * x)]).astype(np.float64)
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert isinstance(g, SciQLopLineGraph)
    assert g.line_count() == 3


def test_line_graph_data_round_trip(plot):
    x = np.linspace(0, 1, 50).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    g = plot.plot(x, y, graph_type=GraphType.Line)
    data = g.data()
    assert len(data) == 2
    np.testing.assert_array_equal(np.array(data[0]), x)
    np.testing.assert_array_equal(np.array(data[1]), y)


def test_line_graph_row_major_2d(plot):
    x = np.linspace(0, 1, 100).astype(np.float64)
    y = np.ascontiguousarray(np.column_stack([np.sin(x), np.cos(x)]))  # row-major
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert g.line_count() == 2


def test_line_graph_column_major_2d(plot):
    x = np.linspace(0, 1, 100).astype(np.float64)
    y = np.asfortranarray(np.column_stack([np.sin(x), np.cos(x)]))  # col-major
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert g.line_count() == 2


def test_line_graph_float32_y(plot):
    x = np.linspace(0, 1, 50).astype(np.float64)
    y = np.sin(x).astype(np.float32)
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert g.line_count() == 1

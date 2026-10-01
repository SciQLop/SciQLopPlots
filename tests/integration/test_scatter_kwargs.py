"""Keyword arguments reach the right C++ parameter.

The typesystem renamed SciQLopPlotCollectionInterface.scatter's arguments in the wrong
order: Python saw "plot_type: GraphMarkerShape, marker: PlotType". A keyword call such as
panel.scatter(x, y, plot_type=PlotType.TimeSeries) then failed with "wrong argument
values", and with it every panel.plot(..., graph_type=GraphType.Scatter) in SciQLop.
"""
import inspect

import numpy as np
import pytest
from shibokensupport.signature import get_signature

import SciQLopPlots
from SciQLopPlots import (
    GraphMarkerShape,
    PlotType,
    SciQLopMultiPlotPanel,
    SciQLopTimeSeriesPlot,
)

ARG_TYPES = {"marker": "GraphMarkerShape", "plot_type": "PlotType"}


def _signatures(method):
    sig = get_signature(method)
    return sig if isinstance(sig, list) else [sig] if sig else []


def _bound_methods():
    for cls_name, cls in vars(SciQLopPlots.SciQLopPlotsBindings).items():
        if not inspect.isclass(cls):
            continue
        for name, member in vars(cls).items():
            if callable(member) and not name.startswith("_"):
                yield f"{cls_name}.{name}", member


# Parsing every signature trips shiboken's warnings on defaults it cannot evaluate.
@pytest.mark.filterwarnings("ignore:pyside_type_init:RuntimeWarning")
def test_marker_and_plot_type_arguments_have_their_own_types():
    wrong = []
    for qualname, method in _bound_methods():
        for sig in _signatures(method):
            for arg, type_name in ARG_TYPES.items():
                param = sig.parameters.get(arg)
                if param is not None and type_name not in str(param.annotation):
                    wrong.append(f"{qualname}: {arg}: {param.annotation}")
    assert wrong == []


def _xy():
    x = np.linspace(0.0, 10.0, 50)
    return x, np.sin(x)


def test_panel_scatter_takes_plot_type_and_marker_by_keyword(panel):
    x, y = _xy()
    plot, graph = panel.scatter(
        x, y, plot_type=PlotType.TimeSeries, marker=GraphMarkerShape.Circle
    )
    assert isinstance(plot, SciQLopTimeSeriesPlot)
    assert graph is not None


def test_panel_scatter_callable_takes_plot_type_and_marker_by_keyword(panel):
    plot, graph = panel.scatter(
        lambda start, stop: _xy(), plot_type=PlotType.TimeSeries, marker=GraphMarkerShape.Circle
    )
    assert isinstance(plot, SciQLopTimeSeriesPlot)
    assert graph is not None


def test_panel_plot_dispatches_scatter_with_plot_type(panel):
    from SciQLopPlots import GraphType

    x, y = _xy()
    plot, graph = panel.plot(x, y, graph_type=GraphType.Scatter, plot_type=PlotType.TimeSeries)
    assert isinstance(plot, SciQLopTimeSeriesPlot)
    assert graph is not None


def test_new_panel_fixture_is_a_multi_plot_panel(panel):
    assert isinstance(panel, SciQLopMultiPlotPanel)

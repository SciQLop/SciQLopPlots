"""The same factory takes the same keywords on a plot and on a panel."""
import numpy as np
import pytest

from SciQLopPlots import GraphMarkerShape

X = np.linspace(0, 1, 10)
Z = np.random.default_rng(0).random((10, 4))


def _graph(result):
    return result[1] if isinstance(result, tuple) else result


def _y(factory):
    return Z if factory == "waterfall" else X


@pytest.mark.parametrize("factory", ["line", "parametric_curve", "waterfall"])
def test_panel_factories_take_marker(qtbot, panel, factory):
    assert _graph(getattr(panel, factory)(X, _y(factory), marker=GraphMarkerShape.Circle)) is not None


@pytest.mark.parametrize("host", ["plot", "panel"])
@pytest.mark.parametrize("factory", ["line", "scatter", "parametric_curve", "waterfall"])
def test_factories_take_name(qtbot, request, host, factory):
    target = request.getfixturevalue(host)
    assert _graph(getattr(target, factory)(X, _y(factory), name="n")).objectName() == "n"


@pytest.mark.parametrize("host", ["plot", "panel"])
def test_waterfall_takes_metadata_not_metaData(qtbot, request, host):
    target = request.getfixturevalue(host)
    assert _graph(target.waterfall(X, Z, metadata={"k": 1})) is not None
    with pytest.raises(AttributeError, match="metaData"):
        target.waterfall(X, Z, metaData={"k": 1})


def test_positional_prefix_unchanged(qtbot, plot):
    assert _graph(plot.line(X, X, ["a"])).components()[0].name() == "a"

"""Reproducers for the SciQLop user-API fuzzing campaign findings handed over
to this repo (2026-06-12): garbage data must raise instead of silently
no-oping (report #20), SciQLopPlotRange must not parse garbage strings to NaN
nor pretend NaN is 1970 in its repr, and plot removal must be free of
abstract-method / removePlottable diagnostics noise.
"""
import numpy as np
import pytest
from PySide6.QtCore import qInstallMessageHandler

from SciQLopPlots import GraphType, SciQLopPlotRange


@pytest.fixture
def qt_messages():
    messages = []
    previous = qInstallMessageHandler(
        lambda mode, ctx, msg: messages.append(msg))
    yield messages
    qInstallMessageHandler(previous)


def _x(n=50):
    return np.linspace(0.0, 10.0, n)


class TestGarbageDataRaises:
    """Non-numeric buffers used to be swallowed to an empty PyBuffer at the
    typesystem conversion, then silently no-op'd at set_data. The conversion
    now sets a Python TypeError (the framework aborts the call), so direct
    set_data raises. The provider pipeline path keeps its own drop-with-warning
    guard, since those buffers are built in C++ and never hit the conversion."""

    def test_line_graph_object_dtype_raises(self, plot):
        x = _x()
        graph = plot.line(x, np.sin(x))
        with pytest.raises((TypeError, ValueError, RuntimeError)):
            graph.set_data(x.astype(object), np.sin(x))

    def test_line_graph_string_dtype_raises(self, plot):
        x = _x()
        graph = plot.line(x, np.sin(x))
        with pytest.raises((TypeError, ValueError, RuntimeError)):
            graph.set_data(np.array(["a"] * 50), np.sin(x))

    def test_multigraph_object_dtype_raises(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        graph = plot.line(x, y)
        with pytest.raises((TypeError, ValueError, RuntimeError)):
            graph.set_data(x, y.astype(object))

    def test_curve_object_dtype_raises(self, plot):
        x = _x()
        curve = plot.plot(x, np.sin(x), graph_type=GraphType.ParametricCurve)
        with pytest.raises((TypeError, ValueError, RuntimeError)):
            curve.set_data(x.astype(object), np.sin(x))

    def test_colormap_object_dtype_raises(self, plot):
        x = _x(32)
        y = np.linspace(0.0, 1.0, 16)
        z = np.random.default_rng(0).random((32, 16))
        cm = plot.colormap(x, y, z)
        with pytest.raises((TypeError, ValueError, RuntimeError)):
            cm.set_data(x.astype(object), y, z)

    def test_valid_data_still_works(self, plot):
        x = _x()
        graph = plot.line(x, np.sin(x))
        graph.set_data(x, np.cos(x))  # no exception


class TestRangeStringParsing:
    """SciQLopPlotRange('garbage', 'dates') used to produce NaN bounds with a
    repr formatting NaN as 1970-01-01 (epoch 0)."""

    def test_garbage_date_strings_raise(self):
        with pytest.raises((TypeError, ValueError, RuntimeError)):
            SciQLopPlotRange("garbage", "dates")

    def test_valid_date_strings_still_parse(self):
        r = SciQLopPlotRange("2020-01-01", "2020-01-02")
        assert r.start() < r.stop()
        assert "2020-01-01" in repr(r)

    def test_nan_time_range_repr_does_not_pretend_epoch(self):
        r = SciQLopPlotRange(float("nan"), float("nan"), True)
        assert "1970" not in repr(r)
        assert "nan" in repr(r).lower()


class TestRemovalDiagnostics:
    """Removing a plot fired 'Abstract method called: ... plotRemoved' from
    the axis synchronizers, and teardown paths spammed
    'removePlottable ... not in list'."""

    def test_remove_plot_is_diagnostic_clean(self, panel, qt_messages, qtbot):
        x = _x()
        plot, _ = panel.plot(x, np.sin(x))
        panel.remove_plot(plot)
        noise = [m for m in qt_messages
                 if "Abstract method called" in m or "not in list" in m]
        assert not noise, f"plot removal emitted diagnostics noise: {noise}"

    def test_graph_replacement_is_diagnostic_clean(self, plot, qt_messages):
        x = _x()
        graph = plot.line(x, np.sin(x))
        with pytest.raises((TypeError, ValueError, RuntimeError)):
            graph.set_data(x.astype(object), np.sin(x))
        noise = [m for m in qt_messages if "not in list" in m]
        assert not noise, f"set_data emitted removePlottable noise: {noise}"

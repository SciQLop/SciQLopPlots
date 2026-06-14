"""Reproducers for the SciQLop user-API fuzzing campaign findings handed over
to this repo (2026-06-12): garbage data must raise instead of silently
no-oping (report #20), SciQLopPlotRange must not parse garbage strings to NaN
nor pretend NaN is 1970 in its repr, and plot removal must be free of
abstract-method / removePlottable diagnostics noise.
"""
import numpy as np
import pytest
from PySide6.QtCore import qInstallMessageHandler

from SciQLopPlots import (
    GraphType,
    SciQLopHorizontalLine,
    SciQLopMultiPlotPanel,
    SciQLopPlotRange,
    SciQLopVerticalLine,
)


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


class TestOverloadErrorMessage:
    """A wrong-argument call on a SciQLopPlotsBindings class must produce a
    real TypeError — not the NameError that shiboken's signature formatter
    throws when it can't resolve the binding module name in its eval namespace
    (report #2 / finding #1)."""

    def test_wrong_args_raise_typeerror_not_nameerror(self):
        # Item ctors are Python-wrapped for Ptr auto-deref (finding #3), which
        # routes around shiboken's tp_init: still a TypeError, never NameError.
        with pytest.raises(TypeError):
            SciQLopHorizontalLine("not a plot", 5.0)

    def test_error_message_lists_supported_signatures(self):
        # An unwrapped overloaded method keeps shiboken's rich error listing.
        from PySide6.QtWidgets import QApplication
        from SciQLopPlots import SciQLopPlot
        QApplication.instance() or QApplication([])
        plot = SciQLopPlot()
        x = _x()
        try:
            plot.colormap(x, np.sin(x))  # 2-arg colormap needs (x, y, z)
        except TypeError as e:
            assert "Supported signatures" in str(e), str(e)
        except NameError as e:
            pytest.fail(f"shiboken signature formatter raised NameError: {e}")


class TestInterfacePtrAutoDeref:
    """``panel.plots()`` returns ``SciQLopPlotInterfacePtr`` (QPointer) wrappers.
    Passing one where a plot pointer is expected used to require a manual
    ``ptr.data()`` — shiboken doesn't implicitly deref the smart pointer
    (finding #3). The typesystem now accepts the Ptr directly."""

    def _panel_with_plot(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x = _x()
        panel.plot(x, np.sin(x))
        return panel

    def test_ptr_accepted_by_item_ctor(self, qtbot):
        panel = self._panel_with_plot(qtbot)
        ptr = panel.plots()[0]
        # SciQLopVerticalLine(SciQLopPlot*, ...) — derived-pointer param
        line = SciQLopVerticalLine(ptr, 5.0)
        assert line is not None

    def test_ptr_accepted_by_base_interface_param(self, qtbot):
        panel = self._panel_with_plot(qtbot)
        ptr = panel.plots()[0]
        # remove_plot(SciQLopPlotInterface*) — base-pointer param
        panel.remove_plot(ptr)
        assert len(panel.plots()) == 0

    def test_raw_plot_still_accepted(self, qtbot):
        panel = self._panel_with_plot(qtbot)
        raw = panel.plots()[0].data()
        line = SciQLopVerticalLine(raw, 5.0)
        assert line is not None


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

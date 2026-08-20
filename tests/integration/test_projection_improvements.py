import pytest
import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import (
    SciQLopPlot,
    SciQLopNDProjectionPlot,
    GraphType,
)
from conftest import process_events


class TestSubplotAccessor:
    """subplot_count() and subplot(i) expose internal plots."""

    def test_default_three_subplots(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        assert proj.subplot_count() == 3

    def test_custom_subplot_count(self, qtbot):
        proj = SciQLopNDProjectionPlot(2)
        qtbot.addWidget(proj)
        assert proj.subplot_count() == 2

    def test_subplot_returns_sciqlop_plot(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        sp = proj.subplot(0)
        assert sp is not None
        assert isinstance(sp, SciQLopPlot)

    def test_subplot_out_of_range_returns_none(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        assert proj.subplot(5) is None
        assert proj.subplot(-1) is None


class TestAxisLabels:
    """set_axis_labels() sets X/Y labels on each subplot."""

    def test_set_axis_labels_three_dimensions(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_axis_labels(["X GSE [Re]", "Y GSE [Re]", "Z GSE [Re]"])
        process_events()
        # Subplot 0: x=dim[0], y=dim[1]
        assert proj.subplot(0).x_axis().label() == "X GSE [Re]"
        assert proj.subplot(0).y_axis().label() == "Y GSE [Re]"
        # Subplot 1: x=dim[1], y=dim[2]
        assert proj.subplot(1).x_axis().label() == "Y GSE [Re]"
        assert proj.subplot(1).y_axis().label() == "Z GSE [Re]"
        # Subplot 2: x=dim[2], y=dim[0] (wraps)
        assert proj.subplot(2).x_axis().label() == "Z GSE [Re]"
        assert proj.subplot(2).y_axis().label() == "X GSE [Re]"

    def test_set_axis_labels_wrong_count_is_noop(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_axis_labels(["A", "B"])  # wrong count
        process_events()
        assert proj.subplot(0).x_axis().label() == ""

    def test_direct_subplot_axis_label(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.subplot(1).x_axis().set_label("Custom X")
        process_events()
        assert proj.subplot(1).x_axis().label() == "Custom X"


class TestEqualAspectRatio:
    """set_equal_aspect_ratio() forces 1:1 scaling on subplots."""

    def test_enable_equal_aspect(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_equal_aspect_ratio(True)
        assert proj.equal_aspect_ratio() is True

    def test_disable_equal_aspect(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_equal_aspect_ratio(True)
        proj.set_equal_aspect_ratio(False)
        assert proj.equal_aspect_ratio() is False

    def test_equal_aspect_preserves_data(self, qtbot):
        """Setting equal aspect after data shouldn't crash."""
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)

        t = np.linspace(0, 10, 50, dtype=np.float64)
        x = np.cos(t).astype(np.float64)
        y = np.sin(t).astype(np.float64)
        z = (t * 0.1).astype(np.float64)

        graph = proj.parametric_curve([t, x, y, z], labels=["a", "b", "c"])
        process_events()

        proj.set_equal_aspect_ratio(True)
        process_events()
        assert proj.equal_aspect_ratio() is True


class TestReferenceLayers:
    """add_reference_curve() adds static curves that don't respond to time changes."""

    def test_add_single_reference_curve(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)

        theta = np.linspace(0, 2 * np.pi, 100, dtype=np.float64)
        x = np.cos(theta)
        y = np.sin(theta)
        z = np.zeros_like(theta)

        ref = proj.add_reference_curve([x, y, z], label="Unit circle")
        process_events()
        assert ref is not None

    def test_reference_curve_survives_time_change(self, qtbot):
        from SciQLopPlots import SciQLopPlotRange
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)

        theta = np.linspace(0, 2 * np.pi, 100, dtype=np.float64)
        x = 5.0 * np.cos(theta)
        y = 5.0 * np.sin(theta)
        z = np.zeros_like(theta)

        ref = proj.add_reference_curve([x, y, z], label="Orbit")
        process_events()

        proj.time_axis().set_range(SciQLopPlotRange(100.0, 200.0))
        process_events()
        assert ref is not None

    def test_multiple_reference_curves(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)

        for r in [1.0, 2.0, 3.0]:
            theta = np.linspace(0, 2 * np.pi, 50, dtype=np.float64)
            x = r * np.cos(theta)
            y = r * np.sin(theta)
            z = np.zeros_like(theta)
            ref = proj.add_reference_curve([x, y, z], label=f"r={r}")
            assert ref is not None
        process_events()

    def test_reference_curve_with_two_projections(self, qtbot):
        proj = SciQLopNDProjectionPlot(2)
        qtbot.addWidget(proj)

        theta = np.linspace(0, 2 * np.pi, 50, dtype=np.float64)
        x = np.cos(theta)
        y = np.sin(theta)

        ref = proj.add_reference_curve([x, y], label="Circle")
        process_events()
        assert ref is not None


class TestTimeColorEncoding:
    """set_time_color_enabled() colors curve segments by time."""

    def test_enable_time_color(self, qtbot):
        from PySide6.QtGui import QColor
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)

        t = np.linspace(0, 10, 200, dtype=np.float64)
        x = np.cos(t).astype(np.float64)
        y = np.sin(t).astype(np.float64)
        z = (t * 0.5).astype(np.float64)

        graph = proj.parametric_curve([t, x, y, z], labels=["a", "b", "c"])
        process_events()

        proj.set_time_color_enabled(True)
        process_events()
        assert proj.time_color_enabled() is True

    def test_disable_time_color(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_time_color_enabled(True)
        proj.set_time_color_enabled(False)
        assert proj.time_color_enabled() is False

    def test_time_color_with_data_no_crash(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)

        t = np.linspace(0, 100, 500, dtype=np.float64)
        x = 10 * np.cos(t * 0.1).astype(np.float64)
        y = 10 * np.sin(t * 0.1).astype(np.float64)
        z = (t * 0.01).astype(np.float64)

        graph = proj.parametric_curve([t, x, y, z], labels=["a", "b", "c"])
        proj.set_time_color_enabled(True)
        process_events()

    def test_time_color_set_gradient_colors(self, qtbot):
        from PySide6.QtGui import QColor
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_time_color_enabled(True)
        proj.set_time_color_gradient(QColor("blue"), QColor("red"))
        process_events()
        assert proj.time_color_enabled() is True


class TestLinkedCrosshairs:
    """Crosshairs link across subplots when enabled."""

    def test_enable_linked_crosshairs(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_linked_crosshairs(True)
        assert proj.linked_crosshairs() is True

    def test_disable_linked_crosshairs(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_linked_crosshairs(True)
        proj.set_linked_crosshairs(False)
        assert proj.linked_crosshairs() is False

    def test_linked_crosshairs_no_crash_with_data(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)

        t = np.linspace(0, 10, 100, dtype=np.float64)
        x = np.cos(t).astype(np.float64)
        y = np.sin(t).astype(np.float64)
        z = (t * 0.1).astype(np.float64)

        proj.parametric_curve([t, x, y, z], labels=["a", "b", "c"])
        proj.set_linked_crosshairs(True)
        process_events()


class TestProjectionFloat32Data:
    """NDProjectionCurves::set_data copied time/scalar buffers via the double-only
    SciQLopPyBuffer::data(), which throws -> std::terminate on float32 (or int)
    buffers. Speasy routinely returns float32, so this crashed the app. Fixed by
    dtype-dispatched copies. These pass only when the crash is gone."""

    @pytest.mark.parametrize("time_dtype", [np.float32, np.int64, np.uint32])
    def test_non_double_time_does_not_crash(self, qtbot, time_dtype):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        n = 50
        t = np.linspace(0, 49, n).astype(time_dtype)   # non-double time -> N+1 path
        # x/y kept float64: the curve resampler is separately double-only.
        x = np.cos(np.linspace(0, 10, n)).astype(np.float64)
        y = np.sin(np.linspace(0, 10, n)).astype(np.float64)
        z = (np.arange(n) * 0.1).astype(np.float64)
        graph = proj.parametric_curve([t, x, y, z], labels=["a", "b", "c"])
        process_events()
        assert graph is not None


class TestTimeColoredReferenceCurve:
    """add_reference_curve() rejected the time-first form it needs to colour by time.

    It required exactly one buffer per subplot and paired them itself, always
    landing on SciQLopNDProjectionCurves::set_data's `2 * curves_count` branch --
    the one that sets no time values. So a reference curve could never be
    time-coloured nor carry a time marker, and callers passing time as a trailing
    array (SciQLop's plot_time_colored_curve did) silently got it plotted as a
    spatial dimension. The `[t, d0..dn-1]` form used by parametric_curve() is now
    accepted and forwarded whole.
    """

    N = 200

    @staticmethod
    def _orbit(n):
        """(t, x, y, z) -- a unit-ish helix on an epoch-scale time axis.

        t is deliberately 9 orders of magnitude away from x/y/z so that a time
        buffer mistaken for a spatial dimension is unmissable on an axis range.
        """
        a = np.linspace(0, 2 * np.pi, n)
        return (
            np.linspace(1.0e9, 1.0e9 + 100.0, n),
            np.cos(a),
            np.sin(a),
            np.linspace(-1.0, 1.0, n),
        )

    def test_time_first_form_is_accepted(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        t, x, y, z = self._orbit(self.N)

        ref = proj.add_reference_curve([t, x, y, z], label="orbit")
        process_events()
        assert ref is not None

    def test_only_n_and_n_plus_one_buffers_are_accepted(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        t, x, y, z = self._orbit(self.N)
        assert proj.add_reference_curve([x, y]) is None
        assert proj.add_reference_curve([t, x, y, z, x]) is None

    def test_time_is_not_plotted_as_a_dimension(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        t, x, y, z = self._orbit(self.N)

        ref = proj.add_reference_curve([t, x, y, z], label="orbit")
        assert ref is not None
        qtbot.waitUntil(lambda: not ref.busy(), timeout=5000)
        process_events()

        for i in range(proj.subplot_count()):
            proj.subplot(i).rescale_axes()
        process_events()

        for i in range(proj.subplot_count()):
            for name, axis in (("x", proj.subplot(i).x_axis()),
                               ("y", proj.subplot(i).y_axis())):
                r = axis.range()
                assert r.start() < -0.9 and r.stop() > 0.9, (
                    f"subplot {i} {name} axis {r.start()}..{r.stop()} does not "
                    "span the trajectory")
                assert abs(r.start()) < 2.0 and abs(r.stop()) < 2.0, (
                    f"subplot {i} {name} axis {r.start()}..{r.stop()} looks like "
                    "the time buffer")

    def test_reference_curve_colours_by_time(self, qtbot, tmp_path):
        from PySide6.QtGui import QColor, QImage

        def hues(plot, name):
            path = tmp_path / f"{name}.png"
            assert plot.save_png(str(path), 500, 400) is True
            img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
            assert not img.isNull()
            return len({img.pixelColor(px, py).hue() // 10
                        for py in range(img.height())
                        for px in range(img.width())
                        if img.pixelColor(px, py).saturation() > 100
                        and img.pixelColor(px, py).value() > 60})

        t, x, y, z = self._orbit(self.N)
        plain, tinted = SciQLopNDProjectionPlot(3), SciQLopNDProjectionPlot(3)
        for p in (plain, tinted):
            qtbot.addWidget(p)
            ref = p.add_reference_curve([t, x, y, z], label="orbit")
            assert ref is not None
            qtbot.waitUntil(lambda: not ref.busy(), timeout=5000)
            process_events()  # the resampled data reaches the curve on a queued signal
            for i in range(p.subplot_count()):
                p.subplot(i).legend().set_visible(False)
                p.subplot(i).rescale_axes()
        tinted.set_time_color_gradient(QColor("blue"), QColor("red"))
        tinted.set_time_color_enabled(True)
        process_events()

        assert hues(plain.subplot(0), "plain") <= 2, "baseline is not single-hued"
        assert hues(tinted.subplot(0), "tinted") > 4


class TestProjectionGraphIsConcrete:
    """A callable on a projection plot came back as the base interface.

    Shiboken only exposes a class listed in the typesystem, and
    SciQLopNDProjectionCurves was not, so `panel.plot(callable,
    plot_type=Projections)` returned something typed SciQLopGraphInterface.
    That hides observe(), which is how a function graph gets bound to a time
    axis -- so a projection graph could never be driven and never fetched.
    """

    def test_a_callable_projection_graph_exposes_observe(self, qtbot):
        from SciQLopPlots import SciQLopMultiPlotPanel, PlotType

        panel = SciQLopMultiPlotPanel(None, synchronize_x=False,
                                      synchronize_time=True)
        qtbot.addWidget(panel)
        _plot, graph = panel.plot(
            lambda start, stop: [np.linspace(start, stop, 10)] * 4,
            labels=["x", "y", "z"], plot_type=PlotType.Projections)
        process_events()
        assert type(graph).__name__ == "SciQLopNDProjectionCurvesFunction"
        assert hasattr(graph, "observe")

    def test_observing_a_time_axis_drives_the_callable(self, qtbot):
        """The whole point of exposing observe(): the callable now runs."""
        from SciQLopPlots import SciQLopMultiPlotPanel, PlotType, SciQLopPlotRange

        calls = []

        def trajectory(start, stop):
            calls.append((start, stop))
            n = 20
            a = np.linspace(0, 2 * np.pi, n)
            return [np.linspace(start, stop, n), np.cos(a), np.sin(a),
                    np.linspace(-1.0, 1.0, n)]

        panel = SciQLopMultiPlotPanel(None, synchronize_x=False,
                                      synchronize_time=True)
        qtbot.addWidget(panel)
        _proj, graph = panel.plot(trajectory, labels=["x", "y", "z"],
                                  plot_type=PlotType.Projections)
        ts, _ = panel.plot(lambda a, b: [np.linspace(a, b, 10), np.zeros(10)],
                           labels=["v"])
        process_events()

        graph.observe(ts.time_axis())
        panel.set_time_axis_range(SciQLopPlotRange(1.0e9, 1.0e9 + 3600))
        qtbot.waitUntil(lambda: len(calls) > 0, timeout=5000)
        assert calls, "the projection callable was never invoked"

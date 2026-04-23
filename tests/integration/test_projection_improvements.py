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

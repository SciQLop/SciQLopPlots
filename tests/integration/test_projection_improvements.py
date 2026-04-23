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

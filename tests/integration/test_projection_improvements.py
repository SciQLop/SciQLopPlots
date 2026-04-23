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

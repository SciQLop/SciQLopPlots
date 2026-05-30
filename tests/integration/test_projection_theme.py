"""Reproducer for D-002: SciQLopNDProjectionPlot must forward theming to its
sub-plots, like SciQLopPlot does.

Before the fix, SciQLopNDProjectionPlot inherited the no-op
SciQLopPlotInterface::set_theme/theme stubs, so theme() returned None and the
child plots in m_plots were never themed (SciQLop worked around this with
findChildren(SciQLopPlot)).
"""
import pytest
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopTheme


@pytest.fixture
def nd_plot(qtbot):
    p = SciQLopNDProjectionPlot()
    qtbot.addWidget(p)
    return p


def test_theme_returns_set_theme(nd_plot):
    theme = SciQLopTheme.dark()
    nd_plot.set_theme(theme)
    QApplication.processEvents()
    assert nd_plot.theme() is not None


def test_set_theme_propagates_to_subplots(nd_plot):
    assert nd_plot.subplot_count() > 0
    theme = SciQLopTheme.dark()
    nd_plot.set_theme(theme)
    QApplication.processEvents()
    for i in range(nd_plot.subplot_count()):
        sub = nd_plot.subplot(i)
        assert sub is not None
        assert sub.theme() is not None, f"subplot {i} was not themed"

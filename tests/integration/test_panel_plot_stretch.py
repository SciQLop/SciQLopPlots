"""Row heights in a multi-plot panel (SciQLop issue #141, "related gaps").

A projection next to six time series got the same height as each of them, and the
only lever was setMinimumHeight() on the widget. A plot now has a stretch weight,
honoured when the panel is organized.
"""
import pytest

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot
from conftest import process_events


@pytest.fixture
def panel(qtbot):
    p = SciQLopMultiPlotPanel(None, synchronize_x=False, synchronize_time=False)
    qtbot.addWidget(p)
    p.resize(500, 900)
    p.show()
    return p


def _two_plots(panel):
    a, b = SciQLopPlot(), SciQLopPlot()
    panel.add_plot(a)
    panel.add_plot(b)
    process_events()
    return a, b


def test_plots_are_even_by_default(panel):
    a, b = _two_plots(panel)
    panel.organize_plots()
    process_events()
    assert b.height() == pytest.approx(a.height(), rel=0.1)
    assert panel.plot_stretch(a) == 1


def test_a_stretch_of_three_gives_three_times_the_height(panel):
    a, b = _two_plots(panel)
    panel.set_plot_stretch(b, 3)
    process_events()
    assert panel.plot_stretch(b) == 3
    assert b.height() == pytest.approx(3 * a.height(), rel=0.15)


def test_organizing_again_keeps_the_weights(panel):
    a, b = _two_plots(panel)
    panel.set_plot_stretch(b, 3)
    panel.organize_plots()
    process_events()
    assert b.height() == pytest.approx(3 * a.height(), rel=0.15)


def test_a_stretch_below_one_is_clamped(panel):
    a, b = _two_plots(panel)
    panel.set_plot_stretch(b, 0)
    assert panel.plot_stretch(b) == 1

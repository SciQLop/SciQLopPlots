"""API review 3.7 / 2.6: one theme can style several plots and panels; plots signal theme changes."""
import pytest
import shiboken6

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopTheme
from conftest import force_gc, process_events, record


def _plots(qtbot, n, cls=SciQLopPlot):
    made = [cls() for _ in range(n)]
    for p in made:
        qtbot.addWidget(p)
    return made


def test_a_temporary_theme_is_kept(qtbot):
    (plot,) = _plots(qtbot, 1)
    plot.set_theme(SciQLopTheme.dark())
    force_gc()
    process_events()
    assert plot.theme() is not None and shiboken6.isValid(plot.theme())


def test_a_theme_with_an_owner_stays_with_its_owner(qtbot):
    owner, a, b = _plots(qtbot, 3)
    theme = SciQLopTheme.dark(owner)
    a.set_theme(theme)
    b.set_theme(theme)
    assert theme.parent() is owner


def test_panels_share_a_theme(qtbot):
    (first,) = _plots(qtbot, 1, SciQLopMultiPlotPanel)
    second = SciQLopMultiPlotPanel()
    theme = SciQLopTheme.dark(first)
    first.set_theme(theme)
    second.set_theme(theme)
    shiboken6.delete(second)
    process_events()
    assert shiboken6.isValid(theme) and first.theme() is theme


def test_theme_changed_signals(qtbot):
    (plot,) = _plots(qtbot, 1)
    theme = SciQLopTheme.light(plot)
    seen = record(plot.theme_changed)
    plot.set_theme(theme)
    plot.set_theme(theme)
    assert seen == [theme]


def test_a_dying_panel_does_not_signal_its_adopted_theme_going():
    panel = SciQLopMultiPlotPanel()
    panel.set_theme(SciQLopTheme.dark())
    seen = record(panel.theme_changed)
    shiboken6.delete(panel)
    process_events()
    assert seen == []


@pytest.mark.parametrize("cls", [SciQLopPlot, SciQLopMultiPlotPanel])
def test_a_shared_theme_outlives_the_one_that_adopted_it(qtbot, cls):
    first, second = cls(), cls()
    qtbot.addWidget(second)
    theme = SciQLopTheme.dark()
    first.set_theme(theme)
    second.set_theme(theme)
    shiboken6.delete(first)
    process_events()
    assert shiboken6.isValid(theme) and second.theme() is theme

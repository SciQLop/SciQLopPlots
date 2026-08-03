"""SciQLopMultiPlotPanel::set_theme must reach plots living in sub-panels.

Before the fix, set_theme() only walked plots() (direct SciQLopPlotInterface
children) and hooked plot_added on itself, so a sub-panel added with
add_panel()/insert_panel() and every plot created inside it stayed unthemed
(white background under a dark panel).
"""
import pytest

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopTheme


@pytest.fixture
def theme():
    return SciQLopTheme.dark()


@pytest.fixture
def sub_panel(qtbot, panel):
    sub = SciQLopMultiPlotPanel()
    panel.add_panel(sub)
    return sub


def test_existing_sub_panel_is_themed(panel, sub_panel, theme):
    panel.set_theme(theme)
    assert sub_panel.theme() is not None


def test_plots_of_existing_sub_panel_are_themed(panel, sub_panel, theme):
    plot = SciQLopPlot()
    sub_panel.add_plot(plot)
    panel.set_theme(theme)
    assert plot.theme() is not None


def test_sub_panel_added_after_set_theme_is_themed(panel, theme):
    panel.set_theme(theme)
    sub = SciQLopMultiPlotPanel()
    panel.add_panel(sub)
    assert sub.theme() is not None


def test_inserted_sub_panel_is_themed(panel, theme):
    panel.set_theme(theme)
    sub = SciQLopMultiPlotPanel()
    panel.insert_panel(0, sub)
    assert sub.theme() is not None


def test_plot_added_to_sub_panel_after_set_theme_is_themed(panel, sub_panel, theme):
    """The notebook workaround exists because of this: sub-panel plots are
    created after the panel got its theme."""
    panel.set_theme(theme)
    plot = SciQLopPlot()
    sub_panel.add_plot(plot)
    assert plot.theme() is not None


def test_nested_sub_panels_are_themed(panel, sub_panel, theme):
    nested = SciQLopMultiPlotPanel()
    sub_panel.add_panel(nested)
    plot = SciQLopPlot()
    nested.add_plot(plot)
    panel.set_theme(theme)
    assert nested.theme() is not None
    assert plot.theme() is not None


def test_theme_change_reaches_sub_panel(panel, sub_panel, theme):
    panel.set_theme(theme)
    new_theme = SciQLopTheme.light()
    panel.set_theme(new_theme)
    assert sub_panel.theme() is new_theme

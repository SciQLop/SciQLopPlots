"""The legend under SciQLopDrawnObject; its old names are aliases."""
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QCheckBox, QGroupBox

from SciQLopPlots import DelegateRegistry
from conftest import process_events, record


def test_the_old_names_are_aliases(plot):
    legend = plot.legend()
    assert type(legend).staticMetaObject.indexOfSignal("visibility_changed(bool)") == -1
    legend.set_visible(False)
    assert legend.is_visible() is legend.visible() is False


def test_the_old_signal_name_is_the_same_signal(plot):
    legend_class = type(plot.legend())
    assert legend_class.visibility_changed is legend_class.visible_changed


def test_both_signal_names_fire_once_per_change(plot):
    legend = plot.legend()
    legend.set_visible(True)
    seen = record(legend.visibility_changed), record(legend.visible_changed)
    legend.set_visible(False)
    legend.set_visible(True)
    assert seen == ([False, True], [False, True])


def test_the_panel_wide_switch_signals_each_legend(panel):
    import numpy as np
    x = np.linspace(0.0, 10.0, 50)
    panel.plot(x, np.sin(x))
    legend = panel.plots()[0].legend()
    seen = record(legend.visible_changed)
    panel.set_legend_visible(False)
    assert (legend.visible(), seen) == (False, [False])


def test_sciqlop_legend_visible_property_pattern(plot):
    """SciQLop's legend_visible: `legend().is_visible()` and `legend().set_visible(bool(v))`."""
    seen = record(plot.legend().visibility_changed)
    plot.legend().set_visible(bool(0))
    assert plot.legend().is_visible() is False
    plot.legend().set_visible(bool(1))
    assert (plot.legend().is_visible(), seen) == (True, [False, True])


def test_the_inspector_legend_checkbox_follows(qtbot, plot):
    delegate = DelegateRegistry.instance().create_delegate(plot, None)
    qtbot.addWidget(delegate)
    box = next(b for b in delegate.findChildren(QGroupBox) if b.title() == "Legend")
    check = box.findChildren(QCheckBox)[0]
    plot.legend().set_visible(False)
    process_events()
    assert not check.isChecked()


def test_the_legend_colour_signals_once(plot):
    seen = record(plot.legend().color_changed)
    plot.legend().set_color(QColor("red"))
    plot.legend().set_color(QColor("red"))
    assert seen == [QColor("red")]

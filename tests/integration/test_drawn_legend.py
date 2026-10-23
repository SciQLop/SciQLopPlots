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

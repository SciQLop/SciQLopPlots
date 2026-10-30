"""The legend under SciQLopDrawnObject; its old names are aliases."""
import pytest
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QCheckBox, QGroupBox

from SciQLopPlots import DelegateRegistry
from conftest import process_events, record


def test_the_old_names_still_work(plot):
    legend = plot.legend()
    assert type(legend).staticMetaObject.indexOfSignal("visibility_changed(bool)") == -1
    legend.set_visible(False)
    with pytest.warns(DeprecationWarning, match=r"is_visible\(\.\.\.\) is deprecated: use legend\.visible"):
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
    """SciQLop's legend_visible: `legend().is_visible()` (deprecated) and `legend().set_visible(bool(v))`."""
    seen = record(plot.legend().visibility_changed)
    plot.legend().set_visible(bool(0))
    with pytest.warns(DeprecationWarning):
        assert plot.legend().is_visible() is False
    plot.legend().set_visible(bool(1))
    assert (plot.legend().visible(), seen) == (True, [False, True])


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


def _legend_row(qtbot, tmp_path, graph_of, visible):
    """The legend's first row, drawn, for a graph named "g" made by graph_of."""
    from PySide6.QtGui import QImage
    from SciQLopPlots import SciQLopPlot
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    plot.resize(600, 400)
    graph = graph_of(plot)
    graph.set_name("g")
    graph.set_visible(visible)
    # Shown, so the legend is laid out at this size: row_rect(0) of an unshown plot is where the
    # legend sat before the resize, and the probe would compare plain plot background.
    plot.show()
    qtbot.waitExposed(plot)
    plot.replot(True)
    rect = plot.legend().row_rect(0)
    assert rect.isValid(), "no legend row to compare"
    path = str(tmp_path / "legend.png")
    plot.save(path, plot.width(), plot.height())
    return QImage(path).copy(rect)


def test_a_graph_hidden_before_its_first_data_has_a_hidden_graphs_legend_row(qtbot, tmp_path):
    """Review M2 was a false positive: a group row draws no icon for a hidden component, and an
    empty graph has none to draw. NeoQCP's legend ignores the plottable's own visibility."""
    import numpy as np
    x = np.linspace(0.0, 10.0, 50)
    empty = lambda p: p.plot(lambda start, stop: [x[:0], x[:0]])
    with_data = lambda p: p.plot(x, np.sin(x), labels=["a"])
    hidden_empty = _legend_row(qtbot, tmp_path, empty, visible=False)
    hidden = _legend_row(qtbot, tmp_path, with_data, visible=False)
    shown = _legend_row(qtbot, tmp_path, with_data, visible=True)
    assert shown != hidden, "the probe must see a row's icon"
    assert hidden_empty == hidden

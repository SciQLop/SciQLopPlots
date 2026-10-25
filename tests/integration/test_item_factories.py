"""Items belong to their plot: QObject parent, null plot, death with the plot (part B)."""
import gc
import inspect

import pytest
import shiboken6
from PySide6.QtCore import QObject, QPointF, QRectF, SIGNAL, Qt
from PySide6.QtGui import QBrush, QColor, QPen, QPixmap
from PySide6.QtWidgets import QApplication, QWidget
from shibokensupport.signature import get_signature

from SciQLopPlots import (
    SciQLopCurvedLineItem, SciQLopEllipseItem, SciQLopHorizontalLine, SciQLopHorizontalSpan,
    SciQLopPixmapItem, SciQLopPlot, SciQLopPlotRange as R, SciQLopRectangularSpan,
    SciQLopStraightLine, SciQLopTextItem, SciQLopVerticalLine, SciQLopVerticalSpan,
    SciQLopPlotsBindings, SciQLopSpan, SpanKind,
)
from SciQLopPlots import (Coordinates, LineTermination, SciQLopItemInterface,
                          SciQLopNDProjectionPlot)
from SciQLopPlots import MultiPlotsVerticalLine, MultiPlotsVerticalSpan
from conftest import collect, off_thread

CONSTRUCTORS = {
    "text": lambda p: SciQLopTextItem(p, "t", QPointF(10, 10)),
    "pixmap": lambda p: SciQLopPixmapItem(p, QPixmap(4, 4), QRectF(0, 0, 4, 4)),
    "ellipse": lambda p: SciQLopEllipseItem(p, QRectF(0, 0, 4, 4)),
    "curved_line": lambda p: SciQLopCurvedLineItem(p, QPointF(0, 0), QPointF(5, 5)),
    "straight_line": lambda p: SciQLopStraightLine(p, 1.0),
    "vertical_line": lambda p: SciQLopVerticalLine(p, 1.0),
    "horizontal_line": lambda p: SciQLopHorizontalLine(p, 1.0),
    "vertical_span": lambda p: SciQLopVerticalSpan(p, R(1.0, 2.0)),
    "horizontal_span": lambda p: SciQLopHorizontalSpan(p, R(1.0, 2.0)),
    "rectangular_span": lambda p: SciQLopRectangularSpan(p, R(1.0, 2.0), R(1.0, 2.0)),
}


def construct(kind, plot):
    with pytest.warns(DeprecationWarning):
        return CONSTRUCTORS[kind](plot)


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_constructor_parents_the_item_to_its_plot(plot, kind):
    item = construct(kind, plot)
    assert item.parent() is plot


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_constructor_rejects_a_null_plot(kind):
    with pytest.raises(ValueError, match="plot cannot be None"):
        construct(kind, None)


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_held_item_dies_with_its_plot(qtbot, kind):
    plot = SciQLopPlot()
    item = construct(kind, plot)
    shiboken6.delete(plot)
    collect()
    assert not shiboken6.isValid(item)


def _replots(plot):
    qcp = next(w for w in plot.findChildren(QWidget)
               if w.metaObject().indexOfSignal("afterReplot()") >= 0)
    hits = []
    QObject.connect(qcp, SIGNAL("afterReplot()"), lambda: hits.append(True))
    return hits


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_deleting_an_item_repaints_the_plot(plot, kind):
    item = construct(kind, plot)
    for _ in range(3):
        QApplication.processEvents()
    hits = _replots(plot)
    shiboken6.delete(item)
    for _ in range(3):
        QApplication.processEvents()
    assert hits, "the plot was not repainted after the item was detached"


FACTORIES = {
    "text": (lambda p: p.text("t", QPointF(10, 10)), SciQLopTextItem),
    "pixmap": (lambda p: p.pixmap(QPixmap(4, 4), QRectF(0, 0, 4, 4)), SciQLopPixmapItem),
    "ellipse": (lambda p: p.ellipse(QRectF(0, 0, 4, 4)), SciQLopEllipseItem),
    "ellipse_pen": (lambda p: p.ellipse(QRectF(0, 0, 4, 4), QPen(QColor("red")), QBrush(QColor("blue"))),
                    SciQLopEllipseItem),
    "ellipse_colors": (lambda p: p.ellipse(QRectF(0, 0, 4, 4), QColor("red"), 2.0, QColor("blue")),
                       SciQLopEllipseItem),
    "curved_line": (lambda p: p.curved_line(QPointF(0, 0), QPointF(5, 5)), SciQLopCurvedLineItem),
    "straight_line": (lambda p: p.straight_line(1.0, Qt.Orientation.Vertical), SciQLopStraightLine),
    "vertical_line": (lambda p: p.vertical_line(1.0), SciQLopStraightLine),
    "horizontal_line": (lambda p: p.horizontal_line(1.0), SciQLopStraightLine),
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SciQLopSpan),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SciQLopSpan),
    "span": (lambda p: p.span(key_range=R(1.0, 2.0), value_range=R(1.0, 2.0)), SciQLopSpan),
}


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_returns_an_item_of_its_plot(plot, kind):
    make, cls = FACTORIES[kind]
    item = make(plot)
    assert type(item) is cls
    assert item.parent() is plot


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_item_survives_dropping_the_reference(plot, kind):
    make, cls = FACTORIES[kind]
    make(plot)
    gc.collect(); QApplication.processEvents(); gc.collect()
    assert len(plot.findChildren(cls)) == 1


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_item_dies_with_its_plot(qtbot, kind):
    plot = SciQLopPlot()
    item = FACTORIES[kind][0](plot)
    shiboken6.delete(plot)
    collect()
    assert not shiboken6.isValid(item)


def test_factory_off_thread_raises(plot):
    out = {}

    def call():
        try:
            plot.text("t", QPointF(1, 1))
        except RuntimeError as e:
            out["error"] = e
    off_thread(call)
    assert "thread" in str(out.get("error", ""))


def test_factory_keeps_constructor_argument_names(plot):
    span = plot.vertical_span(R(1.0, 2.0), color=QColor("red"), read_only=True,
                              visible=False, tool_tip="tip", coordinates=Coordinates.Data)
    assert span.tool_tip() == "tip"
    line = plot.curved_line(QPointF(0, 0), QPointF(1, 1), start_terminator=LineTermination.Arrow,
                            stop_terminator=LineTermination.NoneTermination)
    assert line is not None


def test_projection_pane_has_the_factories(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    pane = nd.subplot(0)
    item = pane.text("t", QPointF(1, 1))
    assert item.parent() is pane


def test_projection_plot_itself_has_none(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    with pytest.raises(NotImplementedError):
        nd.text("t", QPointF(1, 1))


def _all_subclasses(cls):
    for sub in cls.__subclasses__():
        yield sub
        yield from _all_subclasses(sub)


NO_PLOT_FACTORY = {
    "MultiPlotsVerticalSpan": "panel-only (panel.vertical_span)",
    "MultiPlotsVerticalLine": "panel-only (panel.vertical_line)",
    **dict.fromkeys(
        ["SciQLopVerticalLine", "SciQLopHorizontalLine"],
        "old name (Plan A): only its deprecated constructor builds it"),
}


def test_every_item_class_has_a_factory():
    covered = {cls.__name__ for _, cls in FACTORIES.values()}
    items = {c.__name__ for c in _all_subclasses(SciQLopItemInterface)} | \
            {c.__name__ for c in _all_subclasses(SciQLopStraightLine)} | {"SciQLopStraightLine"}
    abstract = {n for n in items if n.endswith("Interface")}
    missing = items - covered - abstract - set(NO_PLOT_FACTORY)
    assert not missing, f"item classes with no plot factory: {sorted(missing)}"


DEPRECATED = {
    **{k: (v, f"use plot.{k}") for k, v in CONSTRUCTORS.items()},
}


@pytest.mark.parametrize("kind", DEPRECATED)
def test_constructor_warns_and_names_the_factory(plot, kind):
    make, hint = DEPRECATED[kind]
    with pytest.warns(DeprecationWarning, match=hint) as record:
        item = make(plot)
    assert record[0].filename == __file__
    assert shiboken6.isValid(item)


def test_panel_constructors_warn(panel):
    with pytest.warns(DeprecationWarning, match=r"use panel\.vertical_span") as record:
        MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    assert record[0].filename == __file__
    with pytest.warns(DeprecationWarning, match=r"use panel\.vertical_line") as record:
        MultiPlotsVerticalLine(panel, 1.0)
    assert record[0].filename == __file__


def test_factories_do_not_warn(plot, recwarn):
    plot.text("t", QPointF(1, 1))
    plot.vertical_span(R(1.0, 2.0))
    assert not [w for w in recwarn if issubclass(w.category, DeprecationWarning)]


def test_pyproject_filter_turns_a_bare_deprecated_call_into_an_error(plot, panel, sample_data):
    """Pins the filterwarnings entry in pyproject.toml: no pytest.warns wrapper here."""
    with pytest.raises(DeprecationWarning, match=r"is deprecated: use plot\."):
        SciQLopTextItem(plot, "t", QPointF(1, 1))
    graph = plot.line(*sample_data)
    with pytest.raises(DeprecationWarning, match=r"use plot\.remove"):
        plot.remove_plottable(graph)
    with pytest.raises(DeprecationWarning, match=r"use panel\."):
        MultiPlotsVerticalSpan(panel, R(1.0, 2.0))


SPAN_SHORTHANDS = {
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SpanKind.Vertical),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SpanKind.Horizontal),
}


@pytest.mark.parametrize("name", SPAN_SHORTHANDS)
def test_span_shorthands_return_plain_spans(plot, name):
    make, kind = SPAN_SHORTHANDS[name]
    span = make(plot)
    assert (type(span), span.kind(), span.range()) == (SciQLopSpan, kind, R(1.0, 2.0))


def test_rectangular_span_warns_and_returns_a_plain_span(plot):
    with pytest.warns(DeprecationWarning,
                      match=r"rectangular_span\(\.\.\.\) is deprecated: use plot\.span\(\.\.\.\)") as record:
        span = plot.rectangular_span(R(1.0, 2.0), R(3.0, 4.0))
    assert record[0].filename == __file__
    assert (type(span), span.kind(), span.key_range(), span.value_range()) == \
        (SciQLopSpan, SpanKind.Rectangular, R(1.0, 2.0), R(3.0, 4.0))


@pytest.mark.parametrize("name, orientation", [("vertical_line", Qt.Orientation.Vertical),
                                               ("horizontal_line", Qt.Orientation.Horizontal)])
def test_line_shorthands_return_plain_read_only_black_lines(plot, name, orientation):
    line = getattr(plot, name)(1.0)
    assert (type(line), line.orientation(), line.read_only(), line.color()) == \
        (SciQLopStraightLine, orientation, True, QColor(0, 0, 0))


@pytest.mark.parametrize("name", ["vertical_line", "horizontal_line"])
@pytest.mark.parametrize("movable", [True, False])
def test_the_movable_keyword_warns_and_inverts(plot, name, movable):
    with pytest.warns(DeprecationWarning,
                      match=rf"movable=\.\.\. is deprecated: use plot\.{name}\(\.\.\., read_only=\.\.\.\)") as record:
        line = getattr(plot, name)(1.0, movable=movable)
    assert record[0].filename == __file__
    assert line.read_only() is (not movable)


@pytest.mark.parametrize("name", ["vertical_line", "horizontal_line"])
def test_a_bool_second_argument_raises_instead_of_becoming_a_colour(plot, name):
    """Review Focus 1: QColor(True) is a valid colour, so the old movable slot would pass silently."""
    with pytest.raises(TypeError, match="read_only"):
        getattr(plot, name)(1.0, True)


def _parameters(func):
    """Name -> default (or inspect.Parameter.empty), without self."""
    signature = get_signature(inspect.unwrap(func))
    return {name: p.default for name, p in signature.parameters.items() if name != "self"}


SPAN_RANGES = {"key_range", "value_range"}


@pytest.mark.parametrize("base, shorthand, replaced, added", [
    ("span", "vertical_span", SPAN_RANGES, {"range"}),
    ("span", "horizontal_span", SPAN_RANGES, {"range"}),
    ("span", "rectangular_span", set(), set()),
    ("straight_line", "vertical_line", {"orientation"}, set()),
    ("straight_line", "horizontal_line", {"orientation"}, set()),
])
def test_the_plot_shorthands_take_the_base_factory_parameters_and_defaults(
        base, shorthand, replaced, added):
    _assert_shorthand_matches_base(
        SciQLopPlotsBindings.SciQLopPlotInterface, base, shorthand, replaced, added)


def _assert_shorthand_matches_base(cls, base, shorthand, replaced, added):
    base_params, own = _parameters(getattr(cls, base)), _parameters(getattr(cls, shorthand))
    assert set(own) == (set(base_params) - replaced) | added
    shared = (set(own) & set(base_params)) - SPAN_RANGES
    assert {k: own[k] for k in shared} == {k: base_params[k] for k in shared}


PANEL_SHORTHANDS = {
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SciQLopSpan),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SciQLopSpan),
    "vertical_line": (lambda p: p.vertical_line(1.0), SciQLopStraightLine),
    "horizontal_line": (lambda p: p.horizontal_line(0.5), SciQLopStraightLine),
}


@pytest.mark.parametrize("name", PANEL_SHORTHANDS)
def test_panel_shorthands_return_the_plain_classes_at_panel_scope(panel, name):
    make, cls = PANEL_SHORTHANDS[name]
    item = make(panel)
    assert type(item) is cls and item.scope() is panel


def test_the_panel_defaults_are_the_plot_ones(panel):
    span, line = panel.vertical_span(R(1.0, 2.0)), panel.vertical_line(1.0)
    assert (span.color(), span.read_only()) == (QColor(100, 100, 100, 100), False)
    assert (line.color(), line.read_only()) == (QColor(0, 0, 0), True)


@pytest.mark.parametrize("base, shorthand, replaced, added", [
    ("span", "vertical_span", SPAN_RANGES, {"range"}),
    ("span", "horizontal_span", SPAN_RANGES, {"range"}),
    ("straight_line", "vertical_line", {"orientation"}, set()),
    ("straight_line", "horizontal_line", {"orientation"}, set()),
])
def test_the_panel_shorthands_take_the_base_factory_parameters_and_defaults(
        base, shorthand, replaced, added):
    _assert_shorthand_matches_base(
        SciQLopPlotsBindings.SciQLopMultiPlotPanel, base, shorthand, replaced, added)


def test_panels_have_no_rectangular_span(panel):
    assert not hasattr(panel, "rectangular_span")

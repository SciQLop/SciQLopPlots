"""Every drawn object answers visible / selected / name / color the same way.

The contract of docs/plans/2026-10-22-drawn-object-base-design.md, one row per family: a supported
attribute round-trips, signals exactly once, stays silent on the same value, and a set from a
worker thread waits for the GUI thread; an unsupported one raises NotImplementedError naming the
class. Every bound SciQLopDrawnObject subclass is in exactly one row, or listed as abstract.
"""
from collections import Counter
from dataclasses import dataclass

import numpy as np
import pytest
from PySide6.QtGui import QColor

import SciQLopPlots
from SciQLopPlots import (GraphType, SciQLopDrawnObject, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopPlotRange, SciQLopTimeSeriesPlot)
from conftest import off_thread, record

OK, RAISE = "ok", "raise"
X = np.linspace(0.0, 10.0, 50)
Y = np.linspace(0.0, 5.0, 20)
Z = np.random.default_rng(0).random((len(Y), len(X)))
RED, BLUE = QColor(200, 10, 10), QColor(10, 10, 200)
SIGNALS = {"visible": "visible_changed", "selected": "selection_changed",
           "name": "name_changed", "color": "color_changed"}


@dataclass(frozen=True)
class Row:
    """A family: the classes the spec lists, a builder for each concrete class they cover."""
    id: str
    classes: tuple
    builders: dict
    cells: dict
    properties: frozenset = frozenset()  # attributes read and written as Python properties


ROWS: list = []

# Bound bases never built on their own.
ABSTRACT = {"SciQLopDrawnObject"}
# Covered by a row, but no Python factory makes one.
NOT_BUILDABLE: dict = {}


def _ranged(make):
    def made():
        plot = make()
        if plot.x_axis() is not None:
            plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        if plot.time_axis() is not None:
            plot.time_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        return plot
    return made


def _panel():
    panel = SciQLopMultiPlotPanel()
    panel.plot(X, np.sin(X))
    return panel


class Hosts:
    """One of each plot kind, made on first use, each with a range so callbacks fetch."""

    def __init__(self, qtbot):
        self.qtbot = qtbot
        self.keep = []  # Python callables wired to C++ signals must outlive the builder
        self._made = {}

    def _get(self, key, make):
        if key not in self._made:
            widget = make()
            self.qtbot.addWidget(widget)
            self._made[key] = widget
        return self._made[key]

    @property
    def plot(self):
        return self._get("plot", _ranged(SciQLopPlot))

    @property
    def ts_plot(self):
        return self._get("ts_plot", _ranged(SciQLopTimeSeriesPlot))

    @property
    def nd(self):
        return self._get("nd", _ranged(lambda: SciQLopNDProjectionPlot(3)))

    @property
    def panel(self):
        return self._get("panel", _panel)


@pytest.fixture
def hosts(qtbot):
    return Hosts(qtbot)


LINES = np.column_stack([np.sin(X), np.cos(X)])


def _with_components(hosts, graph):
    hosts.qtbot.waitUntil(lambda: len(graph.components()) > 0, timeout=3000)
    return graph


def _shown(hosts, plottable):
    hosts.qtbot.waitUntil(plottable.visible, timeout=3000)
    return plottable


def _fed(hosts, graph, *batch):
    """A remote graph answering every request with `batch`; a new x range asks for one."""
    channel = graph.remote_channel()
    answer = lambda _range: channel.set_data(*batch)
    channel.data_requested.connect(answer)
    hosts.keep.append(answer)
    hosts.plot.x_axis().set_range(SciQLopPlotRange(1.0, 9.0))
    return _with_components(hosts, graph)


def _timeline(hosts):
    timeline = hosts.ts_plot.add_timeline()
    timeline.set_intervals([0.0, 6.0], [5.0, 9.0], lane=["A", "A"])
    return timeline


PLOTTABLE_CELLS = {"visible": OK, "selected": OK, "name": OK, "color": RAISE}

ROWS += [
    Row("graph", ("SciQLopLineGraph", "SciQLopCurve", "SciQLopWaterfallGraph", "SciQLopNDProjectionCurves"), {
        "SciQLopLineGraph": lambda h: h.plot.plot(X, LINES, labels=["a", "b"]),
        "SciQLopLineGraphFunction": lambda h: _with_components(h, h.plot.plot(lambda a, b: [X, LINES])),
        "SciQLopLineGraphRemote": lambda h: _fed(h, h.plot.add_remote_line_graph(["a", "b"]), X, LINES),
        "SciQLopCurve": lambda h: h.plot.plot(X, np.sin(X), graph_type=GraphType.ParametricCurve),
        "SciQLopCurveFunction": lambda h: _with_components(
            h, h.plot.plot(lambda a, b: [X, np.sin(X)], graph_type=GraphType.ParametricCurve)),
        "SciQLopCurveRemote": lambda h: _fed(h, h.plot.add_remote_curve(["c"]), X, np.sin(X)),
        "SciQLopWaterfallGraph": lambda h: h.plot.plot(X, LINES, graph_type=GraphType.Waterfall),
        "SciQLopWaterfallGraphFunction": lambda h: _with_components(
            h, h.plot.plot(lambda a, b: [X, LINES], graph_type=GraphType.Waterfall)),
        "SciQLopWaterfallGraphRemote": lambda h: _fed(h, h.plot.add_remote_waterfall(["a", "b"]), X, LINES),
        "SciQLopNDProjectionCurves": lambda h: _with_components(h, h.nd.plot([X, X, np.sin(X), np.cos(X)])),
        "SciQLopNDProjectionCurvesFunction": lambda h: _with_components(
            h, h.nd.plot(lambda a, b: [X, X, np.sin(X), np.cos(X)], graph_type=GraphType.ParametricCurve)),
    }, PLOTTABLE_CELLS, frozenset({"name"})),
    Row("colour map", ("SciQLopColorMap", "SciQLopHistogram2D"), {
        "SciQLopColorMap": lambda h: h.plot.colormap(X, Y, Z),
        "SciQLopColorMapFunction": lambda h: h.plot.plot(lambda a, b: [X, Y, Z], graph_type=GraphType.ColorMap),
        "SciQLopColorMapRemote": lambda h: h.plot.add_remote_color_map("cm"),
        "SciQLopHistogram2D": lambda h: h.plot.histogram2d(X, np.sin(X)),
        "SciQLopHistogram2DFunction": lambda h: h.plot.plot(lambda a, b: [X, np.sin(X)],
                                                            graph_type=GraphType.Histogram2D),
        "SciQLopHistogram2DRemote": lambda h: h.plot.add_remote_histogram2d("h"),
    }, PLOTTABLE_CELLS, frozenset({"name"})),
    Row("histogram", ("SciQLopHistogram",), {
        "SciQLopHistogram": lambda h: h.plot.histogram(np.sin(X)),
        "SciQLopHistogramFunction": lambda h: _shown(h, h.plot.histogram(lambda a, b: [np.sin(X)])),
    }, PLOTTABLE_CELLS, frozenset({"name"})),
    Row("timeline", ("SciQLopTimeline",), {"SciQLopTimeline": _timeline},
        PLOTTABLE_CELLS, frozenset({"name"})),
]
ABSTRACT |= {"SciQLopPlottableInterface", "SciQLopGraphInterface", "SQPQCPAbstractPlottableWrapper",
             "SciQLopMultiGraphBase", "SciQLopColorMapInterface", "SciQLopColorMapBase"}
NOT_BUILDABLE["SciQLopHistogramRemote"] = "no Python factory makes one"

ROWS.append(Row("component", ("SciQLopGraphComponentInterface",), {
    "SciQLopGraphComponentInterface": lambda h: h.plot.plot(X, np.sin(X), labels=["a"]).components()[0],
}, {"visible": OK, "selected": OK, "name": OK, "color": OK}))


# --- rows: each task inserts its families above this line --------------------------------------
# (parametrize reads ROWS when the test functions below are defined, so rows must come first)


def _bound_subclasses(cls):
    for sub in cls.__subclasses__():
        if sub.__module__.startswith("SciQLopPlots"):
            yield sub
            yield from _bound_subclasses(sub)


def _closure(row):
    return set().union(*({name} | {s.__name__ for s in _bound_subclasses(getattr(SciQLopPlots, name))}
                         for name in row.classes))


def test_every_drawn_class_is_in_exactly_one_row_or_abstract():
    drawn = {c.__name__ for c in _bound_subclasses(SciQLopDrawnObject)}
    reached = Counter(name for row in ROWS for name in _closure(row))
    problems = [f"{n}: in no row" for n in sorted(drawn - set(reached) - ABSTRACT)]
    problems += [f"{n}: in {k} rows" for n, k in reached.items() if k > 1]
    problems += [f"{n}: in a row but not drawn" for n in sorted(set(reached) - drawn)]
    problems += [f"{row.id}: builders {sorted(row.builders)} != {sorted(_closure(row) - set(NOT_BUILDABLE))}"
                 for row in ROWS if set(row.builders) != _closure(row) - set(NOT_BUILDABLE)]
    assert not problems, "\n".join(problems)


def _row(name):
    return next(row for row in ROWS if name in row.builders)


def _get(obj, row, attr):
    value = getattr(obj, attr)
    return value if attr in row.properties else value()


def _set(obj, row, attr, value):
    if attr in row.properties:
        setattr(obj, attr, value)
    else:
        getattr(obj, "set_" + attr)(value)


def _changed(attr, current):
    if attr == "name":
        return current + "-renamed"
    if attr == "color":
        return BLUE if current == RED else RED
    return not current


def _any(attr):
    return {"visible": True, "selected": True, "name": "x", "color": RED}[attr]


def _cases(cell):
    return [pytest.param(name, attr, id=f"{name}-{attr}")
            for row in ROWS for name in row.builders
            for attr, kind in row.cells.items() if kind == cell]


@pytest.mark.parametrize("name, attr", _cases(OK))
def test_a_supported_attribute_round_trips_and_signals_once(hosts, name, attr):
    row = _row(name)
    obj = row.builders[name](hosts)
    value = _changed(attr, _get(obj, row, attr))
    seen = record(getattr(obj, SIGNALS[attr]))
    _set(obj, row, attr, value)
    assert _get(obj, row, attr) == value
    _set(obj, row, attr, value)
    assert seen == [value]


@pytest.mark.parametrize("name, attr", _cases(OK))
def test_a_set_from_a_worker_thread_waits_for_the_gui_thread(hosts, qtbot, name, attr):
    row = _row(name)
    obj = row.builders[name](hosts)
    before = _get(obj, row, attr)
    value = _changed(attr, before)
    seen = record(getattr(obj, SIGNALS[attr]))
    off_thread(lambda: _set(obj, row, attr, value))
    assert (_get(obj, row, attr), seen) == (before, [])
    qtbot.waitUntil(lambda: seen == [value], timeout=2000)
    assert _get(obj, row, attr) == value


@pytest.mark.parametrize("name, attr", _cases(RAISE))
def test_an_unsupported_attribute_raises_naming_the_class(hosts, name, attr):
    obj = _row(name).builders[name](hosts)
    with pytest.raises(NotImplementedError, match=type(obj).__name__):
        getattr(obj, attr)()
    with pytest.raises(NotImplementedError, match=type(obj).__name__):
        getattr(obj, "set_" + attr)(_any(attr))

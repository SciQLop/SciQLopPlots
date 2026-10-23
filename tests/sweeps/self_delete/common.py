"""Shared builders, argument guesses and signal enumeration for the self-delete sweep."""
import enum

import numpy as np
from PySide6.QtCore import QMetaMethod, QPointF, QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QFont, QPen, QPixmap
from PySide6.QtWidgets import QApplication

import SciQLopPlots as S
from SciQLopPlots import (ColorGradient, GraphLineStyle, GraphMarkerShape, GraphType, SciQLopPlotRange as R)

APP = QApplication.instance() or QApplication([])
X = np.linspace(0.0, 10.0, 50)
Y = np.linspace(0.0, 5.0, 20)
Z = np.random.default_rng(0).random((len(Y), len(X)))
LINES = np.column_stack([np.sin(X), np.cos(X), np.sin(2 * X)])
N = len(X)
RED, BLUE = QColor(200, 10, 10), QColor(10, 10, 200)


def pump(n=3):
    for _ in range(n):
        APP.processEvents()


class Hosts:
    def __init__(self):
        self.keep = []
        self._made = {}

    def _get(self, key, make):
        if key not in self._made:
            w = make()
            w.resize(600, 400)
            w.show()
            pump()
            for ax in ("x_axis", "time_axis"):
                a = getattr(w, ax, lambda: None)()
                if a is not None:
                    a.set_range(R(0.0, 10.0))
            pump()
            self._made[key] = w
        return self._made[key]

    @property
    def plot(self):
        return self._get("plot", S.SciQLopPlot)

    @property
    def ts_plot(self):
        return self._get("ts", S.SciQLopTimeSeriesPlot)

    @property
    def nd(self):
        return self._get("nd", lambda: S.SciQLopNDProjectionPlot(3))

    @property
    def panel(self):
        def make():
            p = S.SciQLopMultiPlotPanel()
            p.plot(X, np.sin(X))
            p.plot(X, np.cos(X))
            return p
        return self._get("panel", make)


def _wait(cond, n=200):
    for _ in range(n):
        if cond():
            return
        pump(1)


def _comps(g):
    _wait(lambda: len(g.components()) > 0)
    return g


def _timeline(h):
    tl = h.ts_plot.add_timeline()
    tl.set_intervals([10, 60], [40, 90], lane=["A", "B"], ids=[7, 8])
    return tl


def _first(h, graph):
    h.owner = graph
    return graph.components()[0]


def _decorated(graph):
    graph.set_fill_to_value(0, 0.0)
    graph.set_error_bars(1, 0, 2)
    return graph


def _group(h):
    p = h.plot
    return p.group("g", [p.plot(X, np.sin(X), labels=["a"]), p.plot(X, np.cos(X), labels=["b"])])


# name -> (builder, owner-getter or None). A component's owner is the graph it belongs to.
BUILDERS = {
    "line_graph": lambda h: h.plot.plot(X, LINES, labels=["a", "b", "c"]),
    "curve": lambda h: h.plot.parametric_curve(np.cos(X), np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"]),
    "waterfall": lambda h: h.plot.plot(X, LINES, labels=["a", "b", "c"], graph_type=GraphType.Waterfall),
    "line_graph_decorated": lambda h: _decorated(h.plot.plot(X, LINES, labels=["a", "b", "c"])),
    "line_component_decorated": lambda h: _first(h, _decorated(h.plot.plot(X, LINES, labels=["a", "b", "c"]))),
    "colormap": lambda h: h.plot.colormap(X, Y, Z),
    "histogram": lambda h: h.plot.histogram(np.sin(X)),
    "histogram2d": lambda h: h.plot.histogram2d(X, np.sin(X)),
    "timeline": _timeline,
    "nd_projection": lambda h: _comps(h.nd.plot([X, X, np.sin(X), np.cos(X)])),
    "line_component": lambda h: _first(h, h.plot.plot(X, LINES, labels=["a", "b", "c"])),
    # Three columns: error bars need three distinct components.
    "curve_component": lambda h: _first(h, _comps(h.plot.parametric_curve(
        np.cos(X), np.column_stack([np.sin(X), np.cos(X), np.sin(2 * X)]), labels=["a", "b", "c"]))),
    "waterfall_component": lambda h: _first(h, h.plot.plot(X, LINES, labels=["a", "b", "c"], graph_type=GraphType.Waterfall)),
    "nd_component": lambda h: _first(h, _comps(h.nd.plot([X, X, np.sin(X), np.cos(X)]))),
    "text": lambda h: S.SciQLopTextItem(h.plot, "t", QPointF(10, 10)),
    "ellipse": lambda h: S.SciQLopEllipseItem(h.plot, QRectF(10, 10, 20, 20)),
    "pixmap": lambda h: S.SciQLopPixmapItem(h.plot, QPixmap(4, 4), QRectF(1, 1, 2, 2)),
    "curved_line": lambda h: S.SciQLopCurvedLineItem(h.plot, QPointF(0, 0), QPointF(5, 5)),
    "straight_line": lambda h: S.SciQLopStraightLine(h.plot, 1.0),
    "vertical_line": lambda h: S.SciQLopVerticalLine(h.plot, 1.0),
    "horizontal_line": lambda h: S.SciQLopHorizontalLine(h.plot, 0.5),
    "vspan": lambda h: S.SciQLopVerticalSpan(h.plot, R(1.0, 2.0)),
    "hspan": lambda h: S.SciQLopHorizontalSpan(h.plot, R(1.0, 2.0)),
    "rspan": lambda h: S.SciQLopRectangularSpan(h.plot, R(1.0, 2.0), R(1.0, 2.0)),
    "panel_span": lambda h: S.MultiPlotsVerticalSpan(h.panel, R(1.0, 2.0)),
    "panel_line": lambda h: S.MultiPlotsVerticalLine(h.panel, 2.0),
    "panel_span_collection": lambda h: S.MultiPlotsVSpanCollection(h.panel),
    "plot_group": _group,
    "legend": lambda h: h.plot.legend(),
}

COMPONENT_OWNERS = {"line_component", "line_component_decorated", "curve_component", "waterfall_component", "nd_component"}

SKIP = {"moveToThread", "add_inspector_extension", "remove_inspector_extension", "set_meta_data"}
PREFIXES = ("set_", "clear_", "select_", "add_", "push_", "remove_", "reset_", "update_", "move")


def mutators(obj):
    names = [m for m in dir(type(obj)) if m.startswith(PREFIXES) and m not in SKIP]
    return sorted(n for n in names if callable(getattr(obj, n, None)))


def signals(obj):
    mo = obj.metaObject()
    seen = []
    for i in range(mo.methodCount()):
        m = mo.method(i)
        if m.methodType() != QMetaMethod.MethodType.Signal:
            continue
        name = bytes(m.name()).decode()
        if name in ("destroyed", "objectNameChanged") or name in seen or not hasattr(obj, name):
            continue
        seen.append(name)
    return seen


def _pane_curves(obj):
    """A projection's per-pane curves: their signals never reach the projection's own."""
    if not isinstance(obj, S.SciQLopNDProjectionCurves):
        return []
    nd = obj.parent()
    if not hasattr(nd, "subplot_count"):  # unparented, or not in a projection plot
        return []
    return [(f"pane[{i}]", c) for i in range(nd.subplot_count()) for c in nd.subplot(i).plottables()]


def parts(obj):
    """(label, object) pairs whose signals count as the object's own: itself, its components, its panes."""
    out = [("self", obj)]
    comps = getattr(obj, "components", None)
    if callable(comps):
        try:
            out += [(f"component[{i}]", c) for i, c in enumerate(comps())]
        except Exception:
            pass
    return out + _pane_curves(obj)


def _mutated(v):
    if isinstance(v, bool):
        return [not v]
    if isinstance(v, enum.Enum):
        others = [m for m in type(v) if m != v]
        return others[:1]
    if isinstance(v, int):
        return [v + 1]
    if isinstance(v, float):
        return [v + 1.0 if np.isfinite(v) else 1.0]
    if isinstance(v, str):
        return [v + "-x"]
    if isinstance(v, QColor):
        return [BLUE if v == RED else RED]
    if isinstance(v, R):
        if not (np.isfinite(v.start()) and np.isfinite(v.stop())):
            return [R(1.5, 2.5)]
        return [R(v.start() + 0.5, v.stop() + 0.5)]
    if isinstance(v, QPointF):
        return [QPointF(v.x() + 1, v.y() + 1)]
    if isinstance(v, QRectF):
        return [v.translated(1, 1)]
    if isinstance(v, QPen):
        p = QPen(v)
        p.setWidthF(v.widthF() + 1)
        p.setColor(BLUE if v.color() == RED else RED)
        return [p]
    if isinstance(v, QBrush):
        return [QBrush(BLUE if v.color() == RED else RED)]
    if isinstance(v, QFont):
        f = QFont(v)
        f.setPointSize(max(1, v.pointSize()) + 2)
        return [f]
    if isinstance(v, (list, tuple)) and v and all(isinstance(e, (int, float)) for e in v):
        return [[e + 1 for e in v]]
    return []


def current(obj, name):
    for attr in (name, "is_" + name, name + "_enabled"):
        g = getattr(obj, attr, None)
        if g is None:
            continue
        try:
            return [g() if callable(g) else g]
        except Exception:
            continue
    return []


def _axis_of(obj, which):
    for owner in ("plot", "parent_plot"):
        get = getattr(obj, owner, None)
        if callable(get):
            try:
                p = get()
                return getattr(p, which)()
            except Exception:
                pass
    return None


FALLBACK = [
    (), (True,), (False,), (1.5,), (2,), ("x",), (RED,), (R(1.5, 2.5),), (QPointF(1, 1),), (QRectF(1, 1, 2, 2),),
    ([1.0, 2.0],), (QPen(RED),), (QBrush(RED),), (QFont("Sans", 12),), (ColorGradient.Hot,),
    (GraphLineStyle.NoLine,), (GraphMarkerShape.Circle,), (Qt.AlignmentFlag.AlignLeft,),
    (0,), (0, RED), (0, 1.0), (0, True), (0, 0.0), (0, GraphLineStyle.NoLine), (0, GraphMarkerShape.Circle),
    (1, GraphLineStyle.NoLine), (0, 1), (0, 1, 1), (0.5, 0.5), (1.0, 1.0),
    (np.linspace(1.0, 2.0, N), ColorGradient.Hot), (np.linspace(1.0, 2.0, N),), ([RED, BLUE],),
    ([7],), (["A", "B"],), ([RED, BLUE, RED],), (["x", "y", "z"],), ([1.0, 2.0, 3.0],), (0, 1, 2), (1, 0, 2),
]

DATA_ARGS = {
    "line_graph": [(X, LINES)], "waterfall": [(X, LINES)],
    "curve": [(np.cos(X), np.column_stack([np.sin(X), np.cos(X)]))],
    "line_graph_decorated": [(X, LINES)],
    "colormap": [(X, Y, Z)], "histogram": [(np.cos(X),)], "histogram2d": [(X, np.cos(X))],
    "nd_projection": [(X, X, np.sin(X), np.cos(X)), ([X, X, np.sin(X), np.cos(X)],)],
}

DATA_AND_COLOR = {
    "line_graph": [(X, LINES, np.linspace(1.0, 2.0, N))], "waterfall": [(X, LINES, np.linspace(1.0, 2.0, N))],
    "curve": [(np.cos(X), np.column_stack([np.sin(X), np.cos(X)]), np.linspace(1.0, 2.0, N))],
}


import re
from PySide6 import QtCore, QtGui

SCALARS = {"bool": True, "float": 1.5, "int": 1, "str": "x", "QString": "x", "double": 1.5}


def _value_for(annot):
    first = annot.split("|")[0].strip()
    base = first.split(".")[-1]
    if base in SCALARS:
        return SCALARS[base]
    simple = {"QColor": RED, "QPointF": QPointF(1, 1), "QPoint": QPointF(1, 1), "QRectF": QRectF(1, 1, 2, 2),
              "SciQLopPlotRange": R(1.5, 2.5), "QPen": QPen(RED, 2), "QBrush": QBrush(RED),
              "QFont": QFont("Sans", 12), "QPixmap": QPixmap(4, 4)}
    if base in simple:
        return simple[base]
    if "List" in first or "Sequence" in first or "list" in first:
        inner = re.search(r"\[(.*)\]", first)
        v = _value_for(inner.group(1)) if inner else 1.0
        return [v, v] if not isinstance(v, bool) else [v]
    if "PyBuffer" in first or "ndarray" in first or "numpy" in first:
        return np.linspace(1.0, 2.0, N)
    for mod in (S, Qt, QtCore, QtGui):
        e = getattr(mod, base, None)
        if isinstance(e, type) and issubclass(e, enum.Enum):
            members = list(e)
            return members[1] if len(members) > 1 else members[0]
    raise KeyError(first)


def from_signatures(obj, setter):
    text = ""
    for n in (1, 2, 3, 4):
        try:
            getattr(obj, setter)(*[object()] * n)
        except TypeError as e:
            if "Supported signatures" in str(e):
                text = str(e)
                break
        except Exception:
            pass
    if not text:
        return []
    out = []
    for line in text.split("Supported signatures:")[-1].splitlines():
        m = re.search(r"\((.*)\)\s*$", line.strip())
        if not m:
            continue
        params = [p.strip() for p in re.split(r",(?![^\[]*\])", m.group(1)) if p.strip() not in ("", "/", "*")]
        try:
            args = tuple(_value_for(p.split(":", 1)[1].split("=")[0]) for p in params
                         if ":" in p and "=" not in p)
        except KeyError:
            continue
        out.append(args)
    return out


def _coded_intervals():
    one = lambda v: np.array([v], dtype=np.float64)
    return (one(20.0), one(30.0), one(0.0), ["A"], one(0.0), ["x"], ["l"], one(7.0))


def _with_fill(obj, hosts):
    """set_fill_color raises on a column without a fill: give column 0 one first."""
    graph = getattr(hosts, "owner", None) or obj
    graph.set_fill_to_value(0, 0.0)


# setter -> state the setter needs before it can run at all; applied before any slot is wired.
PREPARE = {"set_fill_color": _with_fill}


def prepare(obj, setter, hosts):
    if setter in PREPARE:
        try:
            PREPARE[setter](obj, hosts)
        except Exception:
            pass  # unsupported here: the setter's own call says so
        pump()


def _axis_hosts(hosts):
    """Every plot that owns axes: the projection host has none itself, only its panes do."""
    for host in hosts._made.values():
        yield host
        count = getattr(host, "subplot_count", None)
        if callable(count):
            yield from (host.subplot(i) for i in range(count()))


def candidates(kind, obj, setter, hosts):
    """Argument tuples to try, most plausible first."""
    out = []
    if setter == "set_data":
        out += DATA_ARGS.get(kind, [])
    if setter == "set_data_and_color":
        out += DATA_AND_COLOR.get(kind, [])
    if kind == "timeline":
        out += {"set_intervals": [((20,), (30,))], "set_data": [(np.array([20.0]), np.array([30.0]))],
                "select_ids": [([7],)], "set_lanes": [(["B", "A"],)], "set_category_order": [(["y", "x"],)],
                "set_stack": [("category",), ("time",)], "set_snap_times": [([15.0, 50.0],)],
                "set_category_colors": [({"x": RED},)], "set_style": [("bars",)],
                "set_edit_modes": [(["move", "resize"],)],
                "set_intervals_coded": [_coded_intervals()],
                "set_data_and_color": [([np.array([20.0]), np.array([30.0])], np.array([1.0]))]}.get(setter, [])
    if setter in ("set_x_axis", "set_y_axis", "set_z_axis"):
        for host in _axis_hosts(hosts):
            for which in ("y2_axis", "x2_axis", "y_axis", "x_axis"):
                ax = getattr(host, which, lambda: None)()
                if ax is not None:
                    out.append((ax,))
    if setter == "set_data_and_color" and kind in DATA_ARGS:
        out += [(list(a), np.linspace(1.0, 2.0, N)) for a in DATA_ARGS[kind][:1]]
    if setter == "set_binned":
        out.append((np.linspace(-1.0, 1.0, 11), np.arange(10.0)))
    if setter == "add_object":
        out.append((hosts.plot,))
    if setter == "remove_object" and "panel" in hosts._made:
        out.append((hosts.panel.plots()[0].data(),))
    owner = getattr(hosts, "owner", None)
    if owner is not None and setter in ("set_error_bars", "set_fill_between"):
        others = owner.components()[1:]
        out.append(tuple((others * 2)[:2]) if setter == "set_error_bars" else (others[0],))
    if setter == "update_plot_list":
        out.append(([],))
    if setter.startswith("set_"):
        out += [(v,) for v in sum((_mutated(v) for v in current(obj, setter[4:])), [])]
    if setter.startswith("set_column_"):
        out += [(0,) + (v,) for v in sum((_mutated(v) for v in current(obj, setter[11:])), [])]
    if setter == "set_time_values":
        out.append((np.linspace(0.0, 1.0, N),))
    if setter == "set_color_values":
        out.append((np.linspace(1.0, 2.0, N),))
    return out + from_signatures(obj, setter) + FALLBACK

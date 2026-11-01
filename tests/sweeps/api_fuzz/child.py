"""Imported by case setups inside the child interpreter: receivers, argument candidates, baseline discovery.

Builds on the self-delete sweep's receivers and argument guessing (tests/sweeps/self_delete/common.py).
"""
import json
import os
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "self_delete"))
import common as C  # noqa: E402
import shiboken6  # noqa: E402
from PySide6.QtCore import QObject  # noqa: E402
from PySide6.QtWidgets import QComboBox, QLabel  # noqa: E402

import SciQLopPlots as S  # noqa: E402

R = S.SciQLopPlotRange
X, N = C.X, C.N
Hosts = C.Hosts


def _panel_plot(h):
    return h.panel.plots()[0]


def _keep(h, obj):
    h.keep.append(obj)
    return obj


def _fed(h, graph, *batch):
    """A remote graph answering every request with `batch`."""
    channel = graph.remote_channel()
    answer = _keep(h, lambda _range: channel.set_data(*batch))
    channel.data_requested.connect(answer)
    h.plot.x_axis().set_range(R(1.0, 9.0))
    return C._comps(graph)


class _Extension(S.InspectorExtension):
    """InspectorExtension is abstract: the smallest concrete one."""

    def section_title(self):
        return "fuzz"

    def priority(self):
        return 0

    def build_widget(self, parent):
        return QLabel("fuzz", parent)


def _pipeline(h):
    from SciQLopPlots.pipeline import Pipeline  # noqa: F401  -- the type the >> chain returns
    return _keep(h, h.plot.x_axis().on.range >> (lambda e: e.value) >> h.ts_plot.x_axis().on.range)


# Receiver kinds on top of the self-delete builders: hosts, axes and the other public objects.
EXTRA_KINDS = {
    "plot": lambda h: h.plot,
    "ts_plot": lambda h: h.ts_plot,
    "nd_plot": lambda h: h.nd,
    "panel": lambda h: h.panel,
    "panel_plot": _panel_plot,
    "x_axis": lambda h: h.plot.x_axis(),
    "y_axis": lambda h: h.plot.y_axis(),
    "time_axis": lambda h: h.ts_plot.time_axis(),
    "range": lambda h: R(1.0, 2.0),
    "line_graph_fn": lambda h: C._comps(h.plot.plot(lambda a, b: [X, C.LINES])),
    "line_graph_remote": lambda h: _fed(h, h.plot.add_remote_line_graph(["a", "b", "c"]), X, C.LINES),
    "curve_fn": lambda h: C._comps(h.plot.plot(lambda a, b: [X, np.sin(X)], graph_type=C.GraphType.ParametricCurve)),
    "curve_remote": lambda h: _fed(h, h.plot.add_remote_curve(["c"]), X, np.sin(X)),
    "waterfall_fn": lambda h: C._comps(h.plot.plot(lambda a, b: [X, C.LINES], graph_type=C.GraphType.Waterfall)),
    "waterfall_remote": lambda h: _fed(h, h.plot.add_remote_waterfall(["a", "b", "c"]), X, C.LINES),
    "nd_projection_fn": lambda h: C._comps(h.nd.plot(lambda a, b: [X, X, np.sin(X), np.cos(X)],
                                                     graph_type=C.GraphType.ParametricCurve)),
    "colormap_fn": lambda h: h.plot.plot(lambda a, b: [X, C.Y, C.Z], graph_type=C.GraphType.ColorMap),
    "colormap_remote": lambda h: h.plot.add_remote_color_map("cm"),
    "histogram2d_fn": lambda h: h.plot.plot(lambda a, b: [X, np.sin(X)], graph_type=C.GraphType.Histogram2D),
    "histogram2d_remote": lambda h: h.plot.add_remote_histogram2d("h"),
    "histogram_fn": lambda h: h.plot.histogram(lambda a, b: [np.sin(X)]),
    "overlay": lambda h: h.plot.overlay(),
    "theme": lambda h: _keep(h, S.SciQLopTheme.light()),
    "nd_pane": lambda h: _keep(h, S.SciQLopNDProjectionPane()),
    "dummy_axis": lambda h: _keep(h, S.SciQLopPlotDummyAxis()),
    "x_sync": lambda h: _keep(h, S.XAxisSynchronizer()),
    "vplots_align": lambda h: S.VPlotsAlign(h.panel),
    "collection_behavior": lambda h: S.SciQLopPlotCollectionBehavior(h.panel),
    "widget_plot": lambda h: _keep(h, S.WidgetPlot(QLabel("x"), on_time_range=lambda a, b: None)),
    "old_hspan": lambda h: C._quiet(S.SciQLopHorizontalSpan, h.plot, R(1.0, 2.0), C.RED, False, True, ""),
    "remote_pipeline": lambda h: S.RemoteDataPipeline(h.plot),
    "data_provider": lambda h: _keep(h, S.DataProviderInterface()),
    "data_provider_worker": lambda h: _keep(h, S.DataProviderWorker()),
    "dnd_callback": lambda h: S.PlotDragNDropCallback("text/plain", False, h.plot),
    "products_model": lambda h: S.ProductsModel.instance(),
    "products_node": lambda h: _keep(h, S.ProductsModelNode("fuzz")),
    "products_flat_filter": lambda h: _keep(h, S.ProductsFlatFilterModel(S.ProductsModel.instance())),
    "products_tree_filter": lambda h: _keep(h, S.ProductsTreeFilterModel()),
    "products_view": lambda h: _keep(h, S.ProductsView()),
    "plots_model": lambda h: _keep(h, S.PlotsModel()),
    "plots_tree_view": lambda h: _keep(h, S.PlotsTreeView()),
    "inspector_view": lambda h: _keep(h, S.InspectorView()),
    "inspector_extension": lambda h: _keep(h, _Extension()),
    "delegate_registry": lambda h: S.DelegateRegistry.instance(),
    "type_registry": lambda h: S.TypeRegistry.instance(),
    "font_delegate": lambda h: _keep(h, S.FontDelegate(C.QFont(), C.RED, None)),
    "property_delegate": lambda h: _keep(h, S.PropertyDelegateBase(_keep(h, QObject()), None)),
    "icons": lambda h: S.Icons,
    "score_merger": lambda h: S.ScoreMerger,
    "subsequence_matcher": lambda h: S.SubsequenceMatcher,
    "query_parser": lambda h: S.QueryParser,
    "matched_xy": lambda h: S.MatchedXY.from_py(X, np.sin(X)),
    "matched_xyz": lambda h: S.MatchedXYZ.from_py(X, C.Y, C.Z.T),
    "pipeline": _pipeline,
    "partial_pipeline": lambda h: _keep(h, h.plot.x_axis().on.range >> (lambda e: None)),
}
KINDS = {**C.BUILDERS, **EXTRA_KINDS}


def build(kind, h):
    obj = KINDS[kind](h)
    C.pump()
    return obj


def mro(obj):
    """The class names obj answers for: a class object (static-only API) answers for itself."""
    cls = obj if isinstance(obj, type) else type(obj)
    return [c.__name__ for c in cls.__mro__]


def _child_panel(obj):
    panel = S.SciQLopMultiPlotPanel()
    obj.add_panel(panel)
    return panel


def _added_widget(obj):
    w = QLabel("w")
    obj.add_widget(w)
    return w


def _saves(ext):
    return lambda h, obj: [(os.path.join(tempfile.gettempdir(), f"sqp_fuzz.{ext}"),)]


_LINES_CB = lambda a, b: [X, C.LINES]  # noqa: E731
_XY_CB = lambda a, b: [X, np.sin(X)]  # noqa: E731
_XYZ_CB = lambda a, b: [X, C.Y, C.Z]  # noqa: E731
# plot_at, not plots()[0]: plots() hands out QPointer wrappers that several panel methods refuse.
_PLOT = lambda h, obj: (obj if isinstance(obj, S.SciQLopMultiPlotPanel) else h.panel).plot_at(0)  # noqa: E731

# Argument tuples the generic guesses can't produce, by method name: name -> (hosts, receiver) -> [args, ...].
OVERRIDES = {
    "plot": lambda h, obj: [(X, np.sin(X)), ([X, X, np.sin(X), np.cos(X)],)],
    "plot_impl": lambda h, obj: [(_XYZ_CB,), (_LINES_CB,)],
    "line": lambda h, obj: [(_LINES_CB,)],
    "scatter": lambda h, obj: [(_LINES_CB,)],
    "waterfall": lambda h, obj: [(_LINES_CB,)],
    "parametric_curve": lambda h, obj: [(_XY_CB,)],
    "projection": lambda h, obj: [(lambda a, b: [X, X, np.sin(X)],)],
    "colormap": lambda h, obj: [(_XYZ_CB,), (X, C.Y, C.Z)],
    "histogram2d": lambda h, obj: [(_XY_CB,), (X, np.sin(X))],
    "histogram": lambda h, obj: [(lambda a, b: [np.sin(X)],), (np.sin(X),)],
    "band": lambda h, obj: [(X, np.sin(X) - 1, np.sin(X) + 1)],
    "error_bars": lambda h, obj: [(X, np.sin(X), np.full(N, 0.1))],
    "bars": lambda h, obj: [(np.linspace(0.0, 1.0, 11), np.arange(10.0))],
    "text": lambda h, obj: [("t", C.QPointF(10, 10))],
    "pixmap": lambda h, obj: [(C.QPixmap(4, 4), C.QRectF(1, 1, 2, 2))],
    "curved_line": lambda h, obj: [(C.QPointF(0, 0), C.QPointF(5, 5))],
    "straight_line": lambda h, obj: [(1.0, C.Qt.Orientation.Vertical)],
    "rectangular_span": lambda h, obj: [(R(1.0, 2.0), R(1.0, 2.0))],
    "save": _saves("png"), "save_png": _saves("png"), "save_jpg": _saves("jpg"), "save_bmp": _saves("bmp"),
    "save_pdf": _saves("pdf"), "save_svg": _saves("svg"),
    "set_theme": lambda h, obj: [(S.SciQLopTheme.dark(),)],
    "add_inspector_extension": lambda h, obj: [(_keep(h, _Extension()),)],
    "remove_inspector_extension": lambda h, obj: [(_keep(h, _Extension()),)],
    "freeze_axis": lambda h, obj: [(obj.x_axis(),)],
    "set_metadata": lambda h, obj: [({"a": 1},)],
    "set_meta_data": lambda h, obj: [({"a": 1},)],
    "remove": lambda h, obj: ([(obj.members()[0],)] if isinstance(obj, S.SciQLopPlotGroup)
                              else [(obj.vertical_line(1.0),)] if hasattr(obj, "vertical_line")
                              else [(_PLOT(h, obj),)]),
    "remove_group": lambda h, obj: [(obj.group("g", [obj.plot(X, np.sin(X), labels=["g"])]),)],
    "add_plot": lambda h, obj: [(S.SciQLopPlot(),)],
    "insert_plot": lambda h, obj: [(0, S.SciQLopPlot())],
    "remove_plot": lambda h, obj: [(_PLOT(h, obj),)],
    "contains": lambda h, obj: [(_PLOT(h, obj),)],
    "index_of": lambda h, obj: [(_PLOT(h, obj),)],
    "indexOf": lambda h, obj: [(_PLOT(h, obj),)],
    "plot_stretch": lambda h, obj: [(_PLOT(h, obj),)],
    "set_plot_stretch": lambda h, obj: [(_PLOT(h, obj), 2)],
    "add_panel": lambda h, obj: [(S.SciQLopMultiPlotPanel(),)],
    "insert_panel": lambda h, obj: [(0, S.SciQLopMultiPlotPanel())],
    "remove_panel": lambda h, obj: [(_child_panel(obj),)],
    "add_widget": lambda h, obj: [(QLabel("w"),)],
    "addWidget": lambda h, obj: [(QLabel("w"),)],
    "remove_widget": lambda h, obj: [(_added_widget(obj),)],
    "removeWidget": lambda h, obj: [(_added_widget(obj),)],
    "register_behavior": lambda h, obj: [(S.SciQLopPlotCollectionBehavior(),)],
    "add_accepted_mime_type": lambda h, obj: [(S.PlotDragNDropCallback("text/plain", False, obj),)],
    "set_dimension_pairs": lambda h, obj: [([(0, 1), (1, 2), (0, 2)],)],
    "set_pane_names": lambda h, obj: [(["a", "b", "c"],), (["a", "b"],), (["a"],)],
    "add_model_curve": lambda h, obj: [(lambda x: [x, x],)],
    "model_curve": lambda h, obj: [(lambda x: [x, x],)],
    "set_number_format": lambda h, obj: [("g", 3)],
    "set_tick_labels": lambda h, obj: [({1.0: "one"},)],
    "couple_range_with": lambda h, obj: [(h.ts_plot.x_axis(),), (h.plot.y_axis(),)],
    "decouple_range_from": lambda h, obj: [(h.ts_plot.x_axis(),), (h.plot.y_axis(),)],
    "add": lambda h, obj: [(h.plot.plot(X, np.sin(X), labels=["m"]),)],
    "addObject": lambda h, obj: [(h.plot,)],
    "removeObject": lambda h, obj: [(h.plot,)],
    "updatePlotList": lambda h, obj: [([],)],
    "update_plot_list": lambda h, obj: [([],)],
    "plotAdded": lambda h, obj: [(h.panel.plot_at(0),)],
    "plotRemoved": lambda h, obj: [(h.panel.plot_at(0),)],
    "resynchronize": lambda h, obj: [(h.panel.plot_at(0),)],
    "panelAdded": lambda h, obj: [(_keep(h, S.SciQLopMultiPlotPanel()),)],
    "panelRemoved": lambda h, obj: [(_keep(h, S.SciQLopMultiPlotPanel()),)],
    "add_node": lambda h, obj: [(["fuzz"], S.ProductsModelNode("leaf"))],
    "data": lambda h, obj: [(obj.index(0, 0),), (obj.index(0, 0), 0)],
    "flags": lambda h, obj: [(obj.index(0, 0),)],
    "mimeData": lambda h, obj: [([obj.index(0, 0)],)],
    "decode_mime_data": lambda h, obj: [(C.QtCore.QMimeData(),)],
    "node_data_changed": lambda h, obj: [(_keep(h, S.ProductsModelNode("n")), [0])],
    "add_child": lambda h, obj: [(S.ProductsModelNode("c"),)],
    "child_row": lambda h, obj: [(_keep(h, S.ProductsModelNode("c")),)],
    "set_query": lambda h, obj: [(S.QueryParser.parse("x"),)],
    "set_external_scores": lambda h, obj: [("sig", {"a": 1.0})],
    "filterAcceptsRow": lambda h, obj: [(0, C.QtCore.QModelIndex())],
    "setSourceModel": lambda h, obj: [(S.ProductsModel.instance(),)],
    "set_data_provider": lambda h, obj: [(_keep(h, S.DataProviderInterface()),)],
    "set_data_colored": lambda h, obj: [([X, np.sin(X)], np.sin(X))],
    "create_delegate": lambda h, obj: [(h.plot,)],
    "add_icon": lambda h, obj: [("sqp_fuzz", C.QPixmap(4, 4))],
    "from_py": lambda h, obj: [(X, np.sin(X)), (X, C.Y, C.Z.T)],
    "call": lambda h, obj: [(h.plot, C.QtCore.QMimeData())],
    "merge": lambda h, obj: [({"a": {"x": 1.0}}, list(S.ScoreMergeStrategy)[0], {}, {}, "")],
    "observe": lambda h, obj: [(h.plot.x_axis(),)],
    "set_callable": lambda h, obj: [(_LINES_CB,), (_XY_CB,), (_XYZ_CB,)],
    "build_widget": lambda h, obj: [(_keep(h, QLabel("p")),)],
    "fit_combo_to_content": lambda h, obj: [(_keep(h, QComboBox()),)],
}


def candidate_args(kind, obj, name, h):
    """Argument tuples to try for obj.name, most plausible first."""
    out = []
    if name in OVERRIDES:
        try:
            out += OVERRIDES[name](h, obj)
        except Exception:  # an override that doesn't fit this receiver just yields nothing
            pass
    try:
        out += C.candidates(kind, obj, name, h)
    except Exception:  # the guesses probe the object; a probe that raises just yields fewer guesses
        out += C.FALLBACK
    return out


def arg_kind(a):
    if isinstance(a, bool):
        return "bool"
    if isinstance(a, (int, float, np.integer, np.floating)):
        return "number"
    if isinstance(a, np.ndarray):
        return "buffer"
    if isinstance(a, str):
        return "str"
    if isinstance(a, (list, tuple)):
        return "sequence"
    if isinstance(a, QObject):
        return "qobject"
    return "value"


def find_baseline(kind, name, h, obj, out_path):
    """Write the index of the first candidate that obj.name accepts (or None), with its argument kinds."""
    cands = candidate_args(kind, obj, name, h)
    found, refused = None, None
    for i, args in enumerate(cands):
        try:
            getattr(obj, name)(*args)
        except NotImplementedError as e:
            refused = str(e)
            continue
        except Exception:
            continue
        found = {"index": i, "arg_kinds": [arg_kind(a) for a in args]}
        break
    with open(out_path, "w") as f:
        json.dump({"kind": kind, "name": name, "found": found, "refused": None if found else refused}, f)

"""Reproducers for the API fuzz findings (docs/plans/2026-11-01-api-fuzzing-findings.md), run out of process."""
import pytest

from tests.sweeps.api_fuzz.catalogue import setup_source
from tests.sweeps.api_fuzz.harness import Case, classify, run_batch


def grade(kind, call, expect, allow=()):
    case = Case(f"{kind}:{call[:60]}", setup_source(kind), "import numpy as np\n" + call, expect, allow)
    [outcome] = run_batch([case])
    return classify(case, outcome)


def assert_contract(kind, call, expect, allow=()):
    verdict = grade(kind, call, expect, allow)
    assert verdict.status == "pass", verdict.detail


# F1: None for an object the C++ needs.
@pytest.mark.parametrize("kind, call", [
    ("panel", "obj.add_panel(None)"),
    ("panel", "obj.insert_panel(0, None)"),
    ("panel", "obj.remove_panel(None)"),
    ("panel", "obj.add_widget(None)"),
    ("panel", "obj.remove_widget(None)"),
    ("panel", "obj.register_behavior(None)"),
    ("x_sync", "obj.plotAdded(None)"),
    ("x_sync", "obj.resynchronize(None)"),
    ("products_model", "obj.add_node(['fuzz'], None)"),
    ("products_model", "obj.decode_mime_data(None)"),
    ("products_model", "obj.node_data_changed(None, [0])"),
    ("products_node", "obj.add_child(None)"),
    ("products_node", "obj.child_row(None)"),
    ("data_provider_worker", "obj.set_data_provider(None)"),
    ("delegate_registry", "obj.create_delegate(None)"),
    ("property_delegate", "obj.fit_combo_to_content(None)"),
])
def test_none_for_a_required_object_is_a_type_error(kind, call):
    assert_contract(kind, call, "type")


# F2: a 0-d array where data is expected.
@pytest.mark.parametrize("kind, call", [
    ("curve", "obj.set_time_values(np.array(1.0))"),
    ("curve", "obj.set_color_values(np.array(1.0))"),
    ("nd_plot", "obj.add_reference_curve(np.array(1.0))"),
    ("remote_pipeline", "obj.set_data([np.array(1.0), np.array(1.0)])"),
    ("data_provider_worker", "obj.set_data([np.array(1.0), np.array(1.0)])"),
])
def test_zero_d_data_is_refused(kind, call):
    assert_contract(kind, call, "ok_or_value", ("TypeError",))


# F3 / F5: bad plot indices, and non-finite or huge numbers for integer arguments.
@pytest.mark.parametrize("kind, call, expect, allow", [
    ("panel", "obj.move_plot(-1, 0)", "value", ()),
    ("panel", "obj.move_plot(99, 0)", "value", ()),
    ("panel", "obj.move_plot(None, 0)", "type", ()),
    ("panel", "obj.insert_plot(float('inf'), S.SciQLopPlot())", "value", ("TypeError",)),
    ("panel", "obj.move_plot(float('nan'), 0)", "value", ("TypeError",)),
    ("x_axis", "obj.set_number_format('g', float('nan'))", "value", ("TypeError",)),
    ("x_axis", "obj.set_number_format('g', 2**63)", "value", ()),
])
def test_bad_numbers_and_indices_are_refused_with_a_message(kind, call, expect, allow):
    assert_contract(kind, call, expect, allow)


# F4: pen widths and font sizes.
@pytest.mark.parametrize("kind, call", [
    (kind, f"obj.{setter}({value})")
    for kind, setter in [("vertical_line", "set_line_width"), ("vspan", "set_line_width"),
                         ("line_graph", "set_line_width"), ("line_component", "set_line_width"),
                         ("curve", "set_line_width"), ("nd_projection", "set_line_width"),
                         ("colormap", "set_contour_width"), ("ellipse", "set_line_width"),
                         ("curved_line", "set_line_width"), ("text", "set_font_size")]
    for value in ("float('nan')", "float('inf')", "-1.0")
])
def test_bad_sizes_are_value_errors(kind, call):
    assert_contract(kind, call, "value")


@pytest.mark.parametrize("kind, call", [("vertical_line", "obj.set_line_width(0.0)"),
                                        ("text", "obj.set_font_size(12.5)")])
def test_good_sizes_still_work(kind, call):
    assert_contract(kind, call, "ok")


# F11: a non-array, or None, where array data is required.
@pytest.mark.parametrize("kind, call", [
    ("line_graph", "obj.set_data(object(), X)"),
    ("line_graph", "obj.set_data({'a': 1}, X)"),
    ("colormap", "obj.set_data(X, object(), C.Z)"),
    ("matched_xy", "S.MatchedXY.from_py(object(), X)"),
    ("line_graph", "obj.set_data(None, X)"),
    ("colormap", "obj.set_data(None, C.Y, C.Z)"),
    ("histogram", "obj.set_values(None)"),
    ("histogram2d", "obj.set_data(None, X)"),
])
def test_non_array_data_is_a_type_error(kind, call):
    assert_contract(kind, call, "type")


# F12: argument errors that C++ threw as std::runtime_error.
@pytest.mark.parametrize("kind, call, expect", [
    ("line_graph", "obj.set_data(np.arange(C.N, dtype=np.int8), C.LINES)", "type"),
    ("colormap", "obj.set_data(np.arange(C.N, dtype=np.int8), C.Y, C.Z)", "type"),
    ("colormap", "obj.set_data(X, C.Y[:-1], C.Z)", "value"),
    ("colormap", "obj.set_data(X[:-1], C.Y, C.Z)", "value"),
    ("colormap", "obj.set_data(X, C.Y, np.asfortranarray(C.Z))", "value"),
    ("histogram2d", "obj.set_data(X, X[:-1])", "value"),
    ("line_graph", "obj.set_labels(['a'])", "value"),
    ("line_graph", "obj.set_colors([C.RED])", "value"),
])
def test_argument_errors_have_the_argument_class(kind, call, expect):
    assert_contract(kind, call, expect)


# F15: what panel.plots() returns is accepted wherever a plot is.
@pytest.mark.parametrize("kind, call", [
    ("panel", "obj.index_of(obj.plots()[0])"),
    ("panel", "obj.indexOf(obj.plots()[0])"),
    ("panel", "obj.plot_stretch(obj.plots()[0])"),
    ("panel", "obj.set_plot_stretch(obj.plots()[0], 2)"),
    ("panel", "obj.remove_widget(obj.plots()[0])"),
    ("x_sync", "obj.plotAdded(h.panel.plots()[0])"),
    ("x_sync", "obj.resynchronize(h.panel.plots()[0])"),
    ("vplots_align", "obj.plotAdded(h.panel.plots()[0])"),
    ("collection_behavior", "obj.plotRemoved(h.panel.plots()[0])"),
])
def test_plots_from_plots_are_accepted(kind, call):
    assert_contract(kind, call, "ok")



def test_plots_returns_plot_objects():
    assert_contract("panel", "assert all(isinstance(p, S.SciQLopPlotInterface) for p in obj.plots())", "ok")


def test_spans_returns_span_objects():
    assert_contract("panel_span_collection",
                    "obj.create_span(R(1.0, 2.0))\nassert all(isinstance(s, S.MultiPlotsVerticalSpan) for s in obj.spans())",
                    "ok")


def test_a_deleted_plot_leaves_the_list():
    assert_contract("panel", "import shiboken6\nshiboken6.delete(obj.plot_at(0))\nC.pump()\n"
                             "assert None not in obj.plots() and len(obj.plots()) == 1", "ok")


# F6: save with no file name.
@pytest.mark.parametrize("kind, call", [
    (kind, f"obj.{method}({name})")
    for kind in ("plot", "panel")
    for method in ("save_png", "save_pdf", "save_svg", "save_jpg", "save_bmp", "save")
    for name in ("''", "None")
])
def test_saving_without_a_file_name_is_a_value_error(kind, call):
    assert_contract(kind, call, "value", ("TypeError",))


# F9: a call the axis can't honour raised nothing and only printed.
@pytest.mark.parametrize("kind, call, expect", [
    ("time_axis", "obj.set_log(True)", "value"),
    ("x_axis", "obj.set_date_format('%H:%M')", "value"),
    ("time_axis", "obj.set_tick_labels({1.0: 'one'})", "value"),
    ("x_axis", "obj.couple_range_with(None)", "type"),
    ("x_axis", "obj.decouple_range_from(None)", "type"),
    ("time_axis", "obj.set_log(False)", "ok"),
    ("time_axis", "obj.set_tick_labels({})", "ok"),
    ("time_axis", "h.ts_plot.toggle_log_scale_hovered_or_selected_axes()", "ok"),
])
def test_axis_calls_it_cannot_honour_raise(kind, call, expect):
    assert_contract(kind, call, expect)


# F16: move_panel did nothing (by index) or refused (by panel).
CHILD_PANEL = "child = S.SciQLopMultiPlotPanel()\nobj.insert_panel(0, child)\nC.pump()\n"


@pytest.mark.parametrize("call, expect", [
    (CHILD_PANEL + "obj.move_panel(child, 2)\nassert obj.index_of(child) == 2", "ok"),
    (CHILD_PANEL + "obj.move_panel(0, 2)\nassert obj.index_of(child) == 2", "ok"),
    (CHILD_PANEL + "obj.move_panel(1, 0)", "value"),
    (CHILD_PANEL + "obj.move_panel(99, 0)", "value"),
    ("obj.move_panel(None, 0)", "type"),
    ("obj.move_panel(S.SciQLopMultiPlotPanel(), 0)", "value"),
])
def test_move_panel(call, expect):
    assert_contract("panel", call, expect)


# J2 (user's answer): a deleted WidgetPlot raises like any deleted object.
@pytest.mark.parametrize("call", ["obj.sizeHint()", "obj.widget", "obj.on_time_range(0.0, 1.0)", "obj.time_axis()",
                                  "obj.x_axis()", "obj.color_palette()", "obj.set_color_palette([C.RED])"])
def test_a_deleted_widget_plot_raises(call):
    assert_contract("widget_plot", f"import shiboken6\nshiboken6.delete(obj)\n{call}", "deleted")


# A failed conversion of one argument left its error pending while the next one converted: SystemError.
@pytest.mark.parametrize("call", ["obj.set_column_line_style(float('inf'), S.GraphLineStyle.Line)",
                                  "obj.set_column_marker_shape(2**63, S.GraphMarkerShape.Circle)"])
def test_a_bad_first_argument_is_reported_not_a_system_error(call):
    assert_contract("line_graph", call, "value", ("TypeError",))


# The Python helpers: None, complex and non-array data are wrong types.
@pytest.mark.parametrize("kind, call", [
    ("plot", "obj.band(X, None, np.sin(X))"),
    ("plot", "obj.band(X, {'a': 1}, np.sin(X))"),
    ("plot", "obj.band(None, np.sin(X), np.cos(X))"),
    ("plot", "obj.band(X, np.zeros(C.N, dtype=complex), np.sin(X))"),
    ("plot", "obj.error_bars(X, np.sin(X), None)"),
    ("plot", "obj.error_bars(X, np.zeros(C.N, dtype=complex), 0.1)"),
    ("plot", "obj.histogram(None)"),
    ("plot", "obj.bars(None, np.arange(10.0))"),
    ("histogram", "obj.set_binned(np.linspace(0, 1, 11), None)"),
    ("timeline", "obj.set_intervals_coded(None, np.array([30.0]), np.array([0.0]), ['A'], np.array([0.0]), ['x'], ['l'], np.array([7.0]))"),
    ("matched_xy", "S.MatchedXY.from_py(X, None)"),
    ("curve", "obj.set_time_values(np.zeros(C.N, dtype=complex))"),
    ("histogram2d", "obj.set_normalization(object())"),
    ("timeline", "obj.snap_to = object()"),
])
def test_helpers_call_bad_data_a_type_error(kind, call):
    assert_contract(kind, call, "type")


@pytest.mark.parametrize("kind, call", [("histogram2d", "obj.set_normalization(7)"), ("timeline", "obj.snap_to = -1.0"),
                                        ("timeline", "obj.snap_to = 'middle'")])
def test_helpers_call_bad_values_a_value_error(kind, call):
    assert_contract(kind, call, "value")


@pytest.mark.parametrize("call", [f"obj.{m}('')" for m in ("save", "save_png", "save_pdf", "save_svg", "save_jpg", "save_bmp")])
def test_a_projection_plot_saving_without_a_file_name_is_a_value_error(call):
    assert_contract("nd_plot", call, "value")


@pytest.mark.parametrize("kind, call", [("vertical_line", "obj.set_line_width(1e308)"),
                                        ("colormap", "obj.set_contour_width(1e39)")])
def test_widths_past_float_range_are_value_errors(kind, call):
    assert_contract(kind, call, "value")


# A list of strings given numbers: PySide's converter printed an error per item and carried on.
@pytest.mark.parametrize("kind, call", [("line_graph", "obj.set_labels([1.0, 2.0, 3.0])"),
                                        ("line_graph", "obj.set_labels(np.arange(3.0))"),
                                        ("timeline", "obj.set_lanes([1, 2])")])
def test_numbers_for_a_list_of_strings_are_a_type_error(kind, call):
    assert_contract(kind, call, "type")


# None where a callback is required built a graph that never fetched.
@pytest.mark.parametrize("kind, call", [
    ("plot", "obj.line(None)"), ("plot", "obj.colormap(None)"), ("plot", "obj.scatter(None)"),
    ("panel", "obj.line(None)"), ("nd_plot", "obj.model_curve(None)"), ("line_graph_fn", "obj.set_callable(None)"),
    ("plot", "obj.plot(None)"), ("panel", "obj.plot(None)"), ("plot", "obj.error_bars(None, X, 0.1)"),
])
def test_none_for_a_callback_or_data_is_a_type_error(kind, call):
    assert_contract(kind, call, "type")


def test_a_curve_given_none_data_is_a_type_error():
    assert_contract("curve", "obj.set_data(None, np.column_stack([X, X]))", "type")


@pytest.mark.parametrize("kind, call", [("vertical_line", "obj.set_line_width(2**63)"),
                                        ("colormap", "obj.set_contour_width(2**40)")])
def test_widths_past_qt_range_are_value_errors(kind, call):
    assert_contract(kind, call, "value")


def test_a_provider_without_get_data_for_a_list_reports_instead_of_printing():
    assert_contract("data_provider", "obj.set_data([X, X])\nC.pump(30)", "ok")


# J3 (user's answer): this module's colour and font arguments refuse what PySide passes on silently.
@pytest.mark.parametrize("kind, call, expect", [
    ("x_axis", "obj.set_label_color('not a colour')", "value"),
    ("line_component", "obj.set_color('not a colour')", "value"),
    ("vspan", "obj.set_color('not a colour')", "value"),
    ("theme", "obj.set_background('not a colour')", "value"),
    ("plot", "obj.vertical_line(1.0, color='not a colour')", "value"),
    ("x_axis", "obj.set_label_font([1.0])", "type"),
    ("text", "obj.set_font([1.0, 'Sans'])", "type"),
    ("x_axis", "obj.set_label_color('red')", "ok"),
    ("x_axis", "obj.set_label_color('#00ff00')", "ok"),
    ("x_axis", "obj.set_label_font('Sans')", "ok"),
    ("x_axis", "obj.set_label_font(['Sans', 'Serif'])", "ok"),
])
def test_colour_names_and_font_families_are_checked(kind, call, expect):
    assert_contract(kind, call, expect)


# Found by the story fuzzer: the helpers passed a strided x through, which the C++ conversion refuses.
@pytest.mark.parametrize("call", ["obj.error_bars(X[::10], np.cos(X[::10]), 0.1)",
                                  "obj.band(X[::2], np.sin(X[::2]) - 1, np.sin(X[::2]) + 1)",
                                  "obj.error_bars(X, np.cos(X)[::-1], 0.1)"])
def test_helpers_take_strided_views(call):
    assert_contract("plot", call, "ok")


# Found by the story fuzzer: an unknown keyword on a method raised AttributeError, not TypeError.
@pytest.mark.parametrize("kind, call", [("plot", "obj.text('t', C.QPointF(1, 1), bogus=1)"),
                                        ("plot", "obj.histogram(X, bogus=1)"),
                                        ("ts_plot", "obj.histogram(X, bins=3)")])
def test_an_unknown_keyword_is_a_type_error(kind, call):
    assert_contract(kind, call, "type")


# Off the GUI thread (SciQLop#147 policy): a mutator queues, a factory refuses with RuntimeError.
OFF_THREAD = """import threading
_raised = []
def _run():
    try:
        CALL
    except BaseException as e:
        _raised.append(e)
_t = threading.Thread(target=_run)
_t.start()
_t.join(10)
if _raised:
    raise _raised[0]
"""


@pytest.mark.parametrize("kind, call, expect", [
    ("plot", "obj.set_name('renamed')", "ok"),
    ("plot", "obj.configure_as_timeline(20)", "ok"),
    ("plot", "obj.line(X, np.sin(X))", "deleted"),
    ("plot", "obj.histogram(np.sin(X))", "deleted"),
    ("plot", "obj.band(X, np.sin(X) - 1, np.sin(X) + 1)", "deleted"),
    ("plot", "obj.bars(np.linspace(0.0, 1.0, 11), np.arange(10.0))", "deleted"),
    ("plot", "obj.add_remote_line_graph(['a'])", "deleted"),
    ("plot", "obj.overlay()", "deleted"),
    ("panel", "obj.plot(X, np.sin(X))", "deleted"),
    ("panel", "obj.histogram(np.sin(X))", "deleted"),
    ("panel", "S.SciQLopPlot()", "deleted"),
    ("panel", "S.SciQLopMultiPlotPanel()", "deleted"),
])
def test_off_the_gui_thread(kind, call, expect):
    # "deleted" grades RuntimeError: here, the factory's refusal off-thread.
    assert_contract(kind, OFF_THREAD.replace("CALL", call), expect)


# J4 (user's answer): one colour map of any kind per plot. histogram2d shares the colour scale.
@pytest.mark.parametrize("first, second", [
    ("obj.histogram2d(X, np.sin(X))", "obj.colormap(X, C.Y, C.Z)"),
    ("obj.colormap(X, C.Y, C.Z)", "obj.histogram2d(X, np.sin(X))"),
    ("obj.histogram2d(X, np.sin(X))", "obj.histogram2d(X, np.cos(X))"),
    ("obj.histogram2d(X, np.sin(X))", "obj.add_remote_histogram2d('h')"),
    ("obj.histogram2d(X, np.sin(X))", "obj.plot(lambda a, b: [X, np.sin(X)], graph_type=S.GraphType.Histogram2D)"),
])
def test_a_second_colour_map_of_any_kind_is_refused(first, second):
    assert_contract("plot", f"{first}\nimport pytest\nwith pytest.raises(RuntimeError, match='already has a colour map'):\n    {second}", "ok")


# Found by opencode: a panel added inside itself, or inside a panel it holds, hung (a widget cycle).
@pytest.mark.parametrize("call", [
    "obj.add_panel(obj)",
    "obj.insert_panel(0, obj)",
    "inner = S.SciQLopMultiPlotPanel()\nobj.add_panel(inner)\ninner.add_panel(obj)",
    "inner = S.SciQLopMultiPlotPanel()\nobj.add_panel(inner)\ninner.add_widget(obj)",
])
def test_a_panel_cannot_hold_itself(call):
    assert_contract("panel", call, "value")

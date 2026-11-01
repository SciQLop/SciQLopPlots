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

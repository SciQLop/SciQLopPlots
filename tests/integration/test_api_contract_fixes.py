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

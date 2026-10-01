"""The fuzzer's state machine only holds the bundles some action targets.

An action drawing from a bundle nobody fills breaks the whole fuzzing module at
collection time (a KeyError in build_state_machine), not just that action.
"""
from tests.fuzzing.fuzzer_actions import FUZZER_ACTIONS


def test_every_consumed_bundle_is_produced():
    produced = {a._ui_meta.target for a in FUZZER_ACTIONS} - {None}
    orphans = sorted(
        (a.__name__, bundle)
        for a in FUZZER_ACTIONS
        for bundle in (a._ui_meta.bundles or {}).values()
        if bundle not in produced
    )
    assert orphans == [], f"actions drawing from bundles no action produces: {orphans}"


def test_rule_with_a_removed_plot_index_is_skipped(fuzzing_panel):
    """Bundles keep every index an action returned, also after the plot was removed.

    Hypothesis then drew index 1 after "remove plot", plot_at(1) was None and the action
    crashed; the saved example kept failing every later run.
    """
    from tests.fuzzing.test_fuzzing import SciQLopPlotsFuzzer

    machine = SciQLopPlotsFuzzer.__new__(SciQLopPlotsFuzzer)
    SciQLopPlotsFuzzer.panel = fuzzing_panel
    machine._init_model()
    try:
        assert machine.add_plot() == 0
        assert machine.add_plot() == 1
        machine.remove_last_plot()
        machine.rapid_zoom_burst(plot_index=1)
    finally:
        machine.teardown()

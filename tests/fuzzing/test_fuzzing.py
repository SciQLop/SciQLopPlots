"""Hypothesis-based stateful fuzzing of SciQLopPlots UI."""
import pytest
from hypothesis.stateful import run_state_machine_as_test

from tests.fuzzing.actions import ActionRegistry
from tests.fuzzing.plot_actions import add_plot, remove_last_plot
from tests.fuzzing.graph_actions import add_line_graph, add_colormap, set_graph_data
from tests.fuzzing.item_actions import add_vertical_span
from tests.fuzzing.axis_actions import set_axis_range, toggle_y_log_scale

# Build a combined registry for the fuzzer — does not mutate any module-level registry
fuzzer_registry = ActionRegistry()
for action in [
    add_plot, remove_last_plot,
    add_line_graph, add_colormap, set_graph_data,
    add_vertical_span,
    set_axis_range, toggle_y_log_scale,
]:
    fuzzer_registry.register(action)

SciQLopPlotsFuzzer = fuzzer_registry.build_state_machine(
    name="SciQLopPlotsFuzzer",
    max_examples=10,
    stateful_step_count=10,
)


@pytest.fixture(scope="module")
def fuzzer_class(fuzzing_panel, qtbot):
    SciQLopPlotsFuzzer.panel = fuzzing_panel
    SciQLopPlotsFuzzer.qtbot = qtbot
    yield SciQLopPlotsFuzzer


def test_ui_fuzzing(fuzzer_class):
    run_state_machine_as_test(fuzzer_class)

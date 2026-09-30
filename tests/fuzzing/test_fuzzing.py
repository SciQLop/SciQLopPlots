"""Hypothesis-based stateful fuzzing of SciQLopPlots UI."""
import pytest
from hypothesis.stateful import run_state_machine_as_test

from tests.fuzzing.actions import ActionRegistry
from tests.fuzzing.fuzzer_actions import FUZZER_ACTIONS

# Build a combined registry for the fuzzer — does not mutate any module-level registry
fuzzer_registry = ActionRegistry()
for action in FUZZER_ACTIONS:
    fuzzer_registry.register(action)

SciQLopPlotsFuzzer = fuzzer_registry.build_state_machine(
    name="SciQLopPlotsFuzzer",
    max_examples=10,
    stateful_step_count=10,
)


@pytest.fixture(scope="module")
def fuzzer_class(fuzzing_panel):
    SciQLopPlotsFuzzer.panel = fuzzing_panel
    yield SciQLopPlotsFuzzer


def test_ui_fuzzing(fuzzer_class):
    run_state_machine_as_test(fuzzer_class)

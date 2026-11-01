"""Which public methods do the stories call? Writes test-reports/story_coverage.md with the ones never
called, so new API gets actions. Informational: it asserts only that the collector sees calls."""
import os

from hypothesis.stateful import run_state_machine_as_test

from tests.fuzzing.actions import ActionRegistry
from tests.fuzzing.coverage import NOT_IN_STORIES, collect_called
from tests.fuzzing.fuzzer_actions import FUZZER_ACTIONS
from tests.sweeps.api_fuzz.catalogue import SKIP, public_methods


def _machine(panel):
    registry = ActionRegistry()
    for action in FUZZER_ACTIONS:
        registry.register(action)
    machine = registry.build_state_machine(name="CoverageFuzzer", max_examples=20, stateful_step_count=20)
    machine.panel = panel
    return machine


def test_story_coverage(fuzzing_panel):
    machine = _machine(fuzzing_panel)
    called = collect_called(lambda: run_state_machine_as_test(machine))
    methods = {(m.cls, m.name) for m in public_methods() if m.name not in SKIP}
    missing = sorted(methods - called - set(NOT_IN_STORIES))
    reports = os.environ.get("SCIQLOP_PLOTS_TEST_REPORTS", "test-reports")
    os.makedirs(reports, exist_ok=True)
    with open(os.path.join(reports, "story_coverage.md"), "w") as f:
        f.write(f"# Story coverage\n\n{len(methods & called)} of {len(methods)} public methods called.\n\n")
        f.writelines(f"- {cls}.{name}\n" for cls, name in missing)
    assert called & methods

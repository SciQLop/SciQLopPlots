"""Hypothesis-based stateful fuzzing of SciQLopPlots UI."""
import os
import subprocess
import sys
import tempfile

import pytest
from hypothesis.stateful import run_state_machine_as_test

from tests.fuzzing.actions import ActionRegistry
from tests.fuzzing.fuzzer_actions import FUZZER_ACTIONS

# CI runs 10 stories of 10 steps. The deep run (SCIQLOP_PLOTS_DEEP_FUZZ=1) runs 100 of 50, in a child
# process: a crash then fails with the dumped story instead of killing pytest.
DEEP_CHILD = os.environ.get("SCIQLOP_PLOTS_DEEP_FUZZ_CHILD") == "1"

# Build a combined registry for the fuzzer — does not mutate any module-level registry
fuzzer_registry = ActionRegistry()
for action in FUZZER_ACTIONS:
    fuzzer_registry.register(action)

SciQLopPlotsFuzzer = fuzzer_registry.build_state_machine(
    name="SciQLopPlotsFuzzer",
    max_examples=100 if DEEP_CHILD else 10,
    stateful_step_count=50 if DEEP_CHILD else 10,
)


@pytest.fixture(scope="module")
def fuzzer_class(fuzzing_panel):
    SciQLopPlotsFuzzer.panel = fuzzing_panel
    yield SciQLopPlotsFuzzer


def test_ui_fuzzing(fuzzer_class):
    run_state_machine_as_test(fuzzer_class)


@pytest.mark.skipif(os.environ.get("SCIQLOP_PLOTS_DEEP_FUZZ") != "1" or DEEP_CHILD,
                    reason="deep story run: SCIQLOP_PLOTS_DEEP_FUZZ=1")
def test_deep_story_fuzz():
    env = {**os.environ, "SCIQLOP_PLOTS_DEEP_FUZZ_CHILD": "1",
           "QT_QPA_PLATFORM": os.environ.get("QT_QPA_PLATFORM", "offscreen")}
    done = subprocess.run([sys.executable, "-m", "pytest", f"{__file__}::test_ui_fuzzing", "-q", "-p", "no:cacheprovider"],
                          capture_output=True, text=True, env=env, cwd=tempfile.gettempdir(), timeout=3600)
    assert done.returncode == 0, (done.stdout + done.stderr)[-6000:]

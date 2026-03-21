import os
import pytest

from tests.fuzzing.actions import StoryRunner


@pytest.fixture(scope="session", autouse=True)
def fuzzing_reports_dir():
    reports_dir = os.environ.get("SCIQLOP_PLOTS_TEST_REPORTS", "test-reports")
    os.makedirs(reports_dir, exist_ok=True)


@pytest.fixture(scope="module")
def fuzzing_panel(qapp):
    """Module-scoped panel for Hypothesis fuzzer (reused across examples)."""
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    panel.show()
    return panel


@pytest.fixture
def story_panel(qtbot):
    """Function-scoped panel for scripted story tests (fresh each test)."""
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    return panel


@pytest.fixture
def story_runner(story_panel):
    runner = StoryRunner(story_panel)
    yield runner
    runner.cleanup()

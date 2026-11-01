"""An error in data that arrives later (a provider, a model curve) is printed once and emitted as fetch_failed.

The caller can't catch it: the user's answer (J1, docs/plans/2026-11-01-api-fuzzing-findings.md) is to keep
the traceback and also emit fetch_failed, so code can react, with the plot still usable.
"""
import pytest

from SciQLopPlots import DataProviderInterface, SciQLopNDProjectionPlot, SciQLopPlotRange


# The traceback goes through sys.excepthook on purpose: pytest-qt would turn it into a failure.
pytestmark = pytest.mark.qt_no_exception_capture


def test_a_provider_without_get_data_reports_fetch_failed(qtbot):
    provider = DataProviderInterface()
    failures = []
    provider.fetch_failed.connect(failures.append)
    provider.set_range(SciQLopPlotRange(0.0, 1.0))
    qtbot.waitUntil(lambda: bool(failures), timeout=3000)
    assert "get_data" in failures[0]


def test_a_model_curve_with_the_wrong_signature_reports_fetch_failed(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.show()
    curve = nd.model_curve(lambda x: [x, x])
    failures = []
    curve.fetch_failed.connect(failures.append)
    nd.time_axis().set_range(SciQLopPlotRange(-2.0, 2.0))
    qtbot.waitUntil(lambda: bool(failures), timeout=3000)
    assert "positional argument" in failures[0]

"""An error in data that arrives later (a provider, a model curve) is printed once and emitted as fetch_failed.

The caller can't catch it: the user's answer (J1, docs/plans/2026-11-01-api-fuzzing-findings.md) is to keep
the traceback and also emit fetch_failed, so code can react, with the plot still usable.
"""
import numpy as np
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


@pytest.mark.parametrize("result, why", [
    (lambda X: np.sin(X), "returned an object of type ndarray, not a list or tuple of arrays"),
    (lambda X: (np.array(["a"] * len(X)), np.array(["b"] * len(X))), "unsupported buffer"),
    (lambda X: {"x": X}, "'data' and 'color'"),
])
def test_a_callback_result_that_cant_be_used_reports_fetch_failed(qtbot, result, why):
    import warnings
    from SciQLopPlots import SciQLopPlot
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    plot.show()
    X = np.linspace(0.0, 10.0, 50)
    graph = plot.plot(lambda start, stop: result(X), labels=["a"])
    failures = []
    graph.fetch_failed.connect(failures.append)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)
        plot.x_axis().set_range(SciQLopPlotRange(1.0, 9.0))
        qtbot.waitUntil(lambda: bool(failures), timeout=3000)
    assert why in failures[0]


@pytest.mark.parametrize("factory, result, why", [
    ("error_bars", lambda x: (x, np.sin(x)), "3 or 4 arrays, got 2"),
    ("error_bars", lambda x: (x, x, x, x, x), "3 or 4 arrays, got 5"),
    ("band", lambda x: (x, x), "3 arrays, got 2"),
])
def test_a_band_or_error_bars_callback_with_the_wrong_count_says_what_it_expects(qtbot, plot, factory, result, why):
    graph = getattr(plot, factory)(lambda start, stop: result(np.linspace(start, stop, 20)))
    failures = []
    graph.fetch_failed.connect(failures.append)
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    qtbot.waitUntil(lambda: bool(failures), timeout=3000)
    assert why in failures[0], failures[0]

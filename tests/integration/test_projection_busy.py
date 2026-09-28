"""A projection graph reports busy until every pane has its data.

SciQLopNDProjectionCurves (what add_reference_curve / add_model_curve return)
drives one curve per pane but never answered busy() itself: it was always
False, so waiting on it returned before the panes' resamplers had delivered,
and a rescale right after saw empty curves (flaky under load).
"""
import numpy as np
from conftest import process_events

from SciQLopPlots import SciQLopNDProjectionPlot


def _orbit(n=20000):
    t = np.linspace(1.7e9, 1.7e9 + 86400.0, n)
    phase = np.linspace(0, 4 * np.pi, n)
    return t, np.cos(phase), np.sin(phase), 0.5 * np.sin(2 * phase)


def _spans_trajectory(proj):
    for i in range(proj.subplot_count()):
        proj.subplot(i).rescale_axes()
    process_events()
    return all(proj.subplot(i).x_axis().range().start() < -0.4
               for i in range(proj.subplot_count()))


def test_reference_curve_is_busy_until_panes_have_data(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    ref = proj.add_reference_curve(list(_orbit()), label="orbit")
    assert ref.busy(), "panes are still resampling"
    qtbot.waitUntil(lambda: not ref.busy(), timeout=5000)
    assert _spans_trajectory(proj)


def test_reference_curve_signals_when_it_is_done(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    ref = proj.add_reference_curve(list(_orbit()), label="orbit")
    with qtbot.waitSignal(ref.busy_changed, timeout=5000,
                          check_params_cb=lambda busy: not busy):
        pass
    assert not ref.busy()


def test_model_curve_is_busy_while_fetching(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    t, x, y, z = _orbit()

    def model(start, stop):
        return [t, x, y, z]

    g = proj.add_model_curve(model, label="model")
    g.set_busy(True)
    assert g.busy(), "a fetch in flight makes the projection busy"
    g.set_busy(False)
    qtbot.waitUntil(lambda: not g.busy(), timeout=5000)

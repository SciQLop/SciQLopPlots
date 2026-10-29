"""API review 3.11: a reversed axis is a flag on the axis; its range stays an ordered interval."""
import pytest

from SciQLopPlots import SciQLopPlotRange
from conftest import off_thread, record

AXES = {"x": lambda p: p.x_axis(), "y": lambda p: p.y_axis(), "y2": lambda p: p.y2_axis()}


@pytest.mark.parametrize("which", AXES)
def test_reversed_round_trips_and_signals_once(plot, which):
    axis = AXES[which](plot)
    axis.set_range(SciQLopPlotRange(1.0, 5.0))
    seen = record(axis.reversed_changed)
    axis.set_reversed(True)
    axis.set_reversed(True)
    assert (axis.reversed(), seen, axis.range()) == (True, [True], SciQLopPlotRange(1.0, 5.0))


def test_reversed_is_observable(plot):
    plot.y_axis().on.reversed.value = True
    assert plot.y_axis().reversed()


def test_set_reversed_from_a_worker_lands_on_the_gui_thread(qtbot, plot):
    off_thread(lambda: plot.y_axis().set_reversed(True))
    qtbot.waitUntil(lambda: plot.y_axis().reversed(), timeout=2000)

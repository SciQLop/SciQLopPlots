"""Time ranges accept every common date type and mean UTC (SciQLop#150).

- datetime64 went through float(): [ns] gave nanoseconds instead of seconds,
  coarser units raised TypeError.
- An aware datetime went through QDateTime and lost its zone, a naive one was
  read in the machine's local time: both shifted by the local UTC offset.
- An ISO string's explicit offset was ignored.
"""
import math
from datetime import date, datetime, timedelta, timezone

import numpy as np
import pytest
from PySide6.QtCore import QDateTime, QTimeZone

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlotRange, SciQLopTimeSeriesPlot

START = 1893456000.0  # 2030-01-01T00:00:00Z
STOP = START + 86400.0
PARIS_WINTER = timezone(timedelta(hours=1))

INPUTS = {
    **{f"datetime64[{u}]": (np.datetime64("2030-01-01", u), np.datetime64("2030-01-02", u))
       for u in ("ns", "us", "ms", "s", "m", "h", "D")},
    "aware datetime": (datetime(2030, 1, 1, 1, tzinfo=PARIS_WINTER),
                       datetime(2030, 1, 2, 1, tzinfo=PARIS_WINTER)),
    "naive datetime is UTC": (datetime(2030, 1, 1), datetime(2030, 1, 2)),
    "date": (date(2030, 1, 1), date(2030, 1, 2)),
    "ISO string with offset": ("2030-01-01T01:00:00+01:00", "2030-01-02T01:00:00+01:00"),
    "ISO string": ("2030-01-01T00:00:00", "2030-01-02"),
    "QDateTime": (QDateTime.fromSecsSinceEpoch(int(START), QTimeZone.UTC),
                  QDateTime.fromSecsSinceEpoch(int(STOP), QTimeZone.UTC)),
    "epoch seconds": (START, STOP),
}


@pytest.mark.parametrize("start, stop", INPUTS.values(), ids=INPUTS.keys())
def test_plot_range(start, stop):
    r = SciQLopPlotRange(start, stop)
    assert (r.start(), r.stop()) == (START, STOP)


def test_dates_make_a_time_range():
    assert SciQLopPlotRange(np.datetime64("2030-01-01"), np.datetime64("2030-01-02"))._is_time_range


def test_nat_is_nan():
    r = SciQLopPlotRange(np.datetime64("NaT", "ns"), np.datetime64("2030-01-02", "ns"))
    assert math.isnan(r.start()) or math.isnan(r.stop())


def test_unparsable_string_still_raises():
    with pytest.raises(Exception, match="unparsable"):
        SciQLopPlotRange("not a date", "2030-01-02")


@pytest.mark.parametrize("start, stop", INPUTS.values(), ids=INPUTS.keys())
def test_axis_set_range(qtbot, start, stop):
    plot = SciQLopTimeSeriesPlot()
    qtbot.addWidget(plot)
    plot.x_axis().set_range(start, stop)
    r = plot.x_axis().range()
    assert (r.start(), r.stop()) == (START, STOP)


def test_time_series_set_time_range(qtbot):
    plot = SciQLopTimeSeriesPlot()
    qtbot.addWidget(plot)
    plot.set_time_range(np.datetime64("2030-01-01", "s"), np.datetime64("2030-01-02", "s"))
    r = plot.time_axis().range()
    assert (r.start(), r.stop()) == (START, STOP)


def test_panel_set_time_axis_range(qtbot):
    panel = SciQLopMultiPlotPanel(synchronize_time=True)
    qtbot.addWidget(panel)
    panel.plot(np.array([START, STOP]), np.array([0.0, 1.0]))
    panel.set_time_axis_range(np.datetime64("2030-01-01", "s"), np.datetime64("2030-01-02", "s"))
    r = panel.time_axis_range()
    assert (r.start(), r.stop()) == (START, STOP)


@pytest.mark.parametrize("bad", ["not a date", object()], ids=["garbage string", "object"])
def test_bad_values_raise_and_leave_the_range_alone(qtbot, bad):
    """The two-argument setters convert in the bindings: a bad value used to throw a C++
    exception outside the call's try block and abort the process."""
    plot = SciQLopTimeSeriesPlot()
    qtbot.addWidget(plot)
    plot.x_axis().set_range(START, STOP)
    for setter in (plot.x_axis().set_range, plot.set_time_range):
        with pytest.raises(TypeError, match="expected a number or a date"):
            setter(bad, STOP)
    r = plot.x_axis().range()
    assert (r.start(), r.stop()) == (START, STOP)

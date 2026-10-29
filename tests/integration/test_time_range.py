"""API review 1.12: time_range() / set_time_range() / time_range_changed on time-series plots and panels."""
from datetime import datetime, timezone

import numpy as np
import pytest

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlotRange
from conftest import record

START = datetime(2025, 4, 12, tzinfo=timezone.utc)
T0 = START.timestamp()


def _host(request, name):
    if name == "ts_plot":
        return request.getfixturevalue(name)
    panel = SciQLopMultiPlotPanel(synchronize_time=True)
    request.getfixturevalue("qtbot").addWidget(panel)
    panel.plot(np.array([T0, T0 + 10.0]), np.array([0.0, 1.0]))
    return panel


@pytest.mark.parametrize("name", ["ts_plot", "panel"])
def test_time_range_round_trips_with_a_signal(request, name):
    host = _host(request, name)
    seen = record(host.time_range_changed)
    host.set_time_range(T0, T0 + 3600.0)
    assert host.time_range() == SciQLopPlotRange(T0, T0 + 3600.0)
    assert seen and seen[-1] == SciQLopPlotRange(T0, T0 + 3600.0)


@pytest.mark.parametrize("name", ["ts_plot", "panel"])
def test_set_time_range_takes_datetimes(request, name):
    host = _host(request, name)
    host.set_time_range(START, "2025-04-12T01:00:00Z")
    assert host.time_range() == SciQLopPlotRange(T0, T0 + 3600.0)


@pytest.mark.parametrize("name", ["ts_plot", "panel"])
def test_time_range_is_observable(request, name):
    host = _host(request, name)
    host.on.time_range.value = SciQLopPlotRange(T0, T0 + 60.0)
    assert host.time_range() == SciQLopPlotRange(T0, T0 + 60.0)

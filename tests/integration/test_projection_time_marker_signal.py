"""`time_marker_changed`: what a layer needs to follow the marker (SciQLop #142).

`set_time_marker()` moved the marker but told nobody, so anything that should react
to "the time under the cursor" had to poll. The signal carries the new time, NaN
when the marker is cleared, and only fires on an actual change.
"""
import math

import pytest

from SciQLopPlots import SciQLopNDProjectionPlot


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    return p


@pytest.fixture
def seen(proj):
    values = []
    proj.time_marker_changed.connect(values.append)
    return values


def test_setting_the_marker_emits_the_time(proj, seen):
    proj.set_time_marker(12.5)
    assert seen == [12.5]


def test_clearing_the_marker_emits_nan(proj, seen):
    proj.set_time_marker(12.5)
    proj.clear_time_marker()
    assert seen[0] == 12.5
    assert len(seen) == 2 and math.isnan(seen[1])


def test_a_nan_time_clears_the_marker(proj, seen):
    proj.set_time_marker(3.0)
    proj.set_time_marker(float("nan"))
    assert len(seen) == 2 and math.isnan(seen[1])


def test_the_same_time_does_not_emit_twice(proj, seen):
    proj.set_time_marker(7.0)
    proj.set_time_marker(7.0)
    assert seen == [7.0]


def test_clearing_an_already_cleared_marker_is_silent(proj, seen):
    proj.clear_time_marker()
    assert seen == []


def test_a_new_time_emits_again(proj, seen):
    proj.set_time_marker(1.0)
    proj.set_time_marker(2.0)
    assert seen == [1.0, 2.0]

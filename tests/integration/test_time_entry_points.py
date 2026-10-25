"""Every time entry point takes every time kind and means UTC epoch seconds (API review 5.7).

Entry points are grouped by what they accept: ranges (this section), scalar positions and
data arrays (further down). INPUT_KINDS maps a label to a factory of the Python value for
2030-01-01T00:00:00Z + offset seconds.
"""
import math
from datetime import date, datetime, timedelta, timezone

import numpy as np
import pytest
from PySide6.QtCore import QDateTime, QTimeZone

from SciQLopPlots import SciQLopPlotRange

T0 = 1893456000.0  # 2030-01-01T00:00:00Z
DAY = 86400.0
PARIS_WINTER = timezone(timedelta(hours=1))


def _utc(seconds):
    return datetime.fromtimestamp(seconds, timezone.utc)


INPUT_KINDS = {
    **{f"datetime64[{u}]": (lambda s, u=u: np.datetime64(int(s), "s").astype(f"datetime64[{u}]"))
       for u in ("ns", "us", "ms", "s", "D")},
    "aware datetime": lambda s: _utc(s).astimezone(PARIS_WINTER),
    "naive datetime": lambda s: _utc(s).replace(tzinfo=None),
    "date": lambda s: _utc(s).date(),
    "ISO string": lambda s: _utc(s).isoformat(),
    "QDateTime": lambda s: QDateTime.fromSecsSinceEpoch(int(s), QTimeZone.UTC),
    "float": float,
}
KIND_IDS = list(INPUT_KINDS)


def pair(kind, a=0.0, b=DAY):
    make = INPUT_KINDS[kind]
    return make(T0 + a), make(T0 + b)


def bounds(r):
    return (r.start(), r.stop())


@pytest.mark.parametrize("kind", KIND_IDS)
@pytest.mark.parametrize("container", [tuple, list], ids=["tuple", "list"])
def test_span_key_range_takes_a_pair(plot, kind, container):
    span = plot.span(key_range=container(pair(kind)))
    assert bounds(span.key_range()) == (T0, T0 + DAY)


@pytest.mark.parametrize("kind", KIND_IDS)
def test_span_value_range_takes_a_pair(plot, kind):
    span = plot.span(key_range=SciQLopPlotRange(0.0, 1.0), value_range=pair(kind))
    assert bounds(span.value_range()) == (T0, T0 + DAY)


def test_span_on_one_axis_keeps_the_other_unbounded(plot):
    span = plot.span(key_range=SciQLopPlotRange(1.0, 2.0))
    assert span.value_range() is None
    assert bounds(span.key_range()) == (1.0, 2.0)


@pytest.mark.parametrize("kind", KIND_IDS)
def test_panel_span_takes_a_pair(panel, kind):
    panel.plot(np.array([T0, T0 + DAY]), np.array([0.0, 1.0]))
    span = panel.span(key_range=pair(kind))
    assert bounds(span.key_range()) == (T0, T0 + DAY)


@pytest.mark.parametrize("kind", KIND_IDS)
def test_vertical_span_takes_a_pair(plot, kind):
    span = plot.vertical_span(pair(kind))
    assert bounds(span.range()) == (T0, T0 + DAY)


@pytest.mark.parametrize("kind", KIND_IDS)
def test_span_set_range_takes_a_pair(plot, kind):
    span = plot.vertical_span(SciQLopPlotRange(0.0, 1.0))
    span.set_range(pair(kind))
    assert bounds(span.range()) == (T0, T0 + DAY)


def test_an_existing_range_still_passes_as_is(plot):
    span = plot.vertical_span(SciQLopPlotRange(1.0, 2.0))
    assert bounds(span.range()) == (1.0, 2.0)


def test_numpy_datetime64_pair_in_a_span_key_range(plot):
    span = plot.span(key_range=(np.datetime64("2030-01-01", "ns"), np.datetime64("2030-01-02", "ns")))
    assert bounds(span.key_range()) == (T0, T0 + DAY)


@pytest.mark.parametrize("bad", [(1.0,), (1.0, 2.0, 3.0), ("nope", "2030-01-02"), (np.datetime64("NaT", "ns"), 1.0)],
                         ids=["one", "three", "garbage", "NaT"])
def test_a_bad_pair_is_rejected(plot, bad):
    with pytest.raises((TypeError, ValueError)):
        plot.span(key_range=bad)

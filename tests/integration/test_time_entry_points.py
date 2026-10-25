"""Every time entry point takes every time kind and means UTC epoch seconds (API review 5.7).

Entry points are grouped by what they accept: ranges (this section), scalar positions and
data arrays (further down). INPUT_KINDS maps a label to a factory of the Python value for
2030-01-01T00:00:00Z + offset seconds.
"""
import math
from datetime import date, datetime, timedelta, timezone

import numpy as np
import pytest
from PySide6.QtCore import QDateTime, Qt, QTimeZone

from conftest import process_events
from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlotRange

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


# --- scalar positions -------------------------------------------------------------------

def _panel_with_a_plot(panel):
    panel.plot(np.array([T0, T0 + DAY]), np.array([0.0, 1.0]))
    return panel


def _crosshair_plot(plot):
    plot.set_crosshair_enabled(True)
    return plot


SCALAR_ENTRY_POINTS = {
    "plot.vertical_line": lambda plot, panel, v: plot.vertical_line(v).position(),
    "plot.horizontal_line": lambda plot, panel, v: plot.horizontal_line(v).position(),
    "plot.straight_line": lambda plot, panel, v: plot.straight_line(v, Qt.Vertical).position(),
    "panel.vertical_line": lambda plot, panel, v: _panel_with_a_plot(panel).vertical_line(v).position(),
    "panel.horizontal_line": lambda plot, panel, v: _panel_with_a_plot(panel).horizontal_line(v).position(),
    "panel.straight_line":
        lambda plot, panel, v: _panel_with_a_plot(panel).straight_line(v, Qt.Vertical).position(),
    "line.set_position": lambda plot, panel, v: _set_position(plot, v),
    "plot.show_crosshair_at_key": lambda plot, panel, v: _crosshair_key(plot, v),
}


def _set_position(plot, v):
    line = plot.vertical_line(0.0)
    line.set_position(v)
    return line.position()


def _crosshair_key(plot, v):
    _crosshair_plot(plot).show_crosshair_at_key(v)
    return plot.crosshair_key()


@pytest.mark.parametrize("kind", KIND_IDS)
@pytest.mark.parametrize("entry", SCALAR_ENTRY_POINTS)
def test_scalar_entry_point_accepts_every_kind(plot, panel, entry, kind):
    assert SCALAR_ENTRY_POINTS[entry](plot, panel, INPUT_KINDS[kind](T0)) == T0


@pytest.mark.parametrize("kind", KIND_IDS)
def test_time_marker_accepts_every_kind(qtbot, kind):
    projection = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(projection)
    projection.set_time_marker(INPUT_KINDS[kind](T0))
    assert projection.time_marker_key() == T0


def test_datetime64_ns_is_seconds_not_nanoseconds(plot):
    assert plot.vertical_line(np.datetime64("2030-01-01", "ns")).position() == T0


@pytest.mark.parametrize("unit", ["s", "D"])
def test_coarse_datetime64_is_accepted_not_a_type_error(plot, unit):
    assert plot.straight_line(np.datetime64("2030-01-01", unit), Qt.Vertical).position() == T0


@pytest.mark.parametrize("entry", ["plot.vertical_line", "line.set_position", "plot.show_crosshair_at_key"])
@pytest.mark.parametrize("bad, error", [(np.datetime64("NaT", "ns"), ValueError), ("not a date", ValueError),
                                        (object(), TypeError)], ids=["NaT", "garbage", "object"])
def test_bad_scalars_raise(plot, panel, entry, bad, error):
    with pytest.raises(error):
        SCALAR_ENTRY_POINTS[entry](plot, panel, bad)


# --- data arrays ------------------------------------------------------------------------

N = 6
Y = np.arange(N, dtype=np.float64)
Y_COLUMNS = np.column_stack([Y, Y + 1.0])
Z = np.arange(N * 3, dtype=np.float64).reshape(N, 3)
Y_AXIS = np.arange(3, dtype=np.float64)


def _graph(result):
    return result[1] if isinstance(result, tuple) else result


def _set_data(host, x):
    graph = _graph(host.line(T0 + np.arange(N, dtype=np.float64), Y))
    graph.set_data(x, Y)
    return graph


def _colormap_set_data(host, x):
    graph = _graph(host.colormap(T0 + np.arange(N, dtype=np.float64), Y_AXIS, Z))
    graph.set_data(x, Y_AXIS, Z)
    return graph


DATA_ENTRY_POINTS = {
    "plot.plot": lambda plot, panel, x: plot.plot(x, Y),
    "plot.line": lambda plot, panel, x: plot.line(x, Y),
    "plot.line columns": lambda plot, panel, x: plot.line(x, Y_COLUMNS),
    "plot.scatter": lambda plot, panel, x: plot.scatter(x, Y),
    "plot.waterfall": lambda plot, panel, x: plot.waterfall(x, Z),
    "plot.colormap": lambda plot, panel, x: plot.colormap(x, Y_AXIS, Z),
    "panel.plot": lambda plot, panel, x: panel.plot(x, Y),
    "panel.line": lambda plot, panel, x: panel.line(x, Y),
    "graph.set_data": lambda plot, panel, x: _set_data(plot, x),
    "colormap.set_data": lambda plot, panel, x: _colormap_set_data(plot, x),
}

ARRAY_KINDS = {
    **{f"datetime64[{u}]": (lambda u=u: (np.datetime64("2030-01-01", "s") + np.arange(N)).astype(f"datetime64[{u}]"))
       for u in ("ns", "us", "ms", "s")},
    "datetime64[D]": lambda: np.datetime64("2030-01-01", "D") + np.arange(N),
    "object array of datetime": lambda: np.array([_utc(T0 + i) for i in range(N)], dtype=object),
    "object array of ISO strings": lambda: np.array([_utc(T0 + i).isoformat() for i in range(N)], dtype=object),
    "object array of dates": lambda: np.array([date(2030, 1, 1)] * N, dtype=object),
}


def _expected_seconds(kind):
    if kind == "datetime64[D]":
        return np.full(N, T0) + np.arange(N) * DAY
    if kind == "object array of dates":
        return np.full(N, T0)
    return T0 + np.arange(N, dtype=np.float64)


def _first_array(graph):
    return np.asarray(graph.data()[0])


@pytest.mark.parametrize("kind", ARRAY_KINDS)
@pytest.mark.parametrize("entry", DATA_ENTRY_POINTS)
def test_data_entry_point_accepts_a_date_array(ts_plot, panel, entry, kind):
    graph = _graph(DATA_ENTRY_POINTS[entry](ts_plot, panel, ARRAY_KINDS[kind]()))
    process_events()
    np.testing.assert_array_equal(_first_array(graph), _expected_seconds(kind))


def test_nat_in_a_data_array_is_a_gap(ts_plot):
    x = np.array(["2030-01-01", "NaT", "2030-01-03"], dtype="datetime64[s]")
    graph = _graph(ts_plot.line(x, np.array([1.0, 2.0, 3.0])))
    process_events()
    seconds = _first_array(graph)
    assert seconds[0] == T0 and math.isnan(seconds[1])


def test_a_non_contiguous_datetime64_array_is_accepted(ts_plot):
    x = (np.datetime64("2030-01-01", "s") + np.arange(2 * N)).astype("datetime64[s]")[::2]
    graph = _graph(ts_plot.line(x, Y))
    process_events()
    np.testing.assert_array_equal(_first_array(graph), T0 + 2.0 * np.arange(N))


def test_an_object_array_that_is_not_dates_is_rejected_with_the_converters_message(ts_plot):
    with pytest.raises(TypeError, match="expected a number or a date, not None"):
        ts_plot.line(np.array([None] * N, dtype=object), Y)


def test_an_object_array_of_bare_numbers_says_so(ts_plot):
    with pytest.raises(TypeError, match="object array"):
        ts_plot.plot(np.array([1.0, 2.0], dtype=object), np.array([1.0, 2.0]))


def test_an_array_like_that_is_not_a_buffer_is_refused(ts_plot):
    class ArrayLike:
        def __array__(self, dtype=None, copy=None):
            return np.arange(N, dtype=np.float64)

    with pytest.raises((TypeError, ValueError)):
        ts_plot.line(ArrayLike(), Y)


def test_a_pandas_series_needs_to_numpy(ts_plot):
    pd = pytest.importorskip("pandas")
    series = pd.Series(np.datetime64("2030-01-01", "s") + np.arange(N))
    with pytest.raises((TypeError, ValueError)):
        ts_plot.line(series, Y)
    graph = _graph(ts_plot.line(series.to_numpy(), Y))
    process_events()
    np.testing.assert_array_equal(_first_array(graph), T0 + np.arange(N, dtype=np.float64))


def test_panel_span_takes_datetime64_and_iso_pairs(panel):
    panel.plot(np.array([T0, T0 + DAY]), np.array([0.0, 1.0]))
    from_datetime64 = panel.span(key_range=(np.datetime64("2030-01-01", "s"), np.datetime64("2030-01-02", "s")))
    from_iso = panel.span(key_range=("2030-01-01", "2030-01-02"))
    assert bounds(from_datetime64.key_range()) == bounds(from_iso.key_range()) == (T0, T0 + DAY)


def test_a_callback_returning_datetime64_is_converted(qtbot, ts_plot):
    seen = []

    def callback(start, stop):
        seen.append(start)
        return np.datetime64("2030-01-01", "s") + np.arange(N), Y

    graph = _graph(ts_plot.line(callback))
    ts_plot.x_axis().set_range(SciQLopPlotRange(T0, T0 + N))
    qtbot.waitUntil(lambda: np.asarray(graph.data()[0] if graph.data() else []).ndim > 0, timeout=5000)
    np.testing.assert_array_equal(_first_array(graph), T0 + np.arange(N, dtype=np.float64))

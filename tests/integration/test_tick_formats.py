"""API review 4.8: tick label formats; a time axis keeps its dates."""
import pytest

from conftest import record

T0 = 1.7e9  # 2023-11-14T22:13:20Z


def _svg(plot):
    return plot.to_bytes("svg", 600, 300)


@pytest.fixture
def timed(ts_plot):
    ts_plot.resize(600, 300)
    ts_plot.time_axis().set_range(T0, T0 + 3600.0)
    return ts_plot


def test_a_time_axis_keeps_its_dates_after_a_log_switch(timed):
    timed.time_axis().set_log(True)
    timed.time_axis().set_log(False)
    assert not timed.time_axis().log() and _svg(timed).count(b"2023/") > 0


def test_date_format_round_trips_signals_and_draws(timed):
    seen = record(timed.time_axis().date_format_changed)
    timed.time_axis().set_date_format("yyyy-MM-dd hh:mm")
    assert (timed.time_axis().date_format(), seen) == ("yyyy-MM-dd hh:mm", ["yyyy-MM-dd hh:mm"])
    svg = _svg(timed)
    assert b"2023-11-14 22:" in svg and b"2023/" not in svg


def test_number_format_round_trips_signals_and_draws(plot):
    plot.resize(600, 300)
    plot.y_axis().set_range(0.0, 1000.0)
    seen = []
    plot.y_axis().number_format_changed.connect(lambda fmt, precision: seen.append((fmt, precision)))
    plot.y_axis().set_number_format("f", 2)
    assert (plot.y_axis().number_format(), plot.y_axis().number_precision(), seen) == ("f", 2, [("f", 2)])
    assert b"200.00" in _svg(plot)


@pytest.mark.parametrize("fmt, precision", [("x", 2), ("gb", 2), ("f", -1)])
def test_a_bad_number_format_raises(plot, fmt, precision):
    with pytest.raises(ValueError):
        plot.y_axis().set_number_format(fmt, precision)


def test_e_keeps_the_axis_powers_of_ten_style(plot):
    import numpy as np
    x = np.linspace(0.0, 1.0, 10)
    plot.resize(600, 300)
    plot.colormap(x, x, np.outer(x, x) * 1e6)
    plot.z_axis().set_number_format("e", 1)
    assert plot.z_axis().number_format() == "e" and b"e+0" not in _svg(plot)


def test_the_powers_of_ten_style_survives_a_round_trip_through_f(plot):
    import numpy as np
    x = np.linspace(0.0, 1.0, 10)
    plot.resize(600, 300)
    plot.colormap(x, x, np.outer(x, x) * 1e6)
    plot.z_axis().set_number_format("f", 1)
    plot.z_axis().set_number_format("e", 1)
    assert b"e+0" not in _svg(plot)

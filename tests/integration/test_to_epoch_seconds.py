"""SciQLopPlots.to_epoch_seconds: the one time conversion behind every time entry point (API review 5.7)."""
import math
from datetime import date, datetime, timedelta, timezone

import numpy as np
import pytest
from PySide6.QtCore import QDateTime, QTimeZone

from SciQLopPlots import to_epoch_seconds

T = 1893456000.0  # 2030-01-01T00:00:00Z
PARIS_WINTER = timezone(timedelta(hours=1))

KINDS = {
    **{f"datetime64[{u}]": np.datetime64("2030-01-01", u) for u in ("ns", "us", "ms", "s", "m", "h", "D")},
    "aware datetime": datetime(2030, 1, 1, 1, tzinfo=PARIS_WINTER),
    "naive datetime is UTC": datetime(2030, 1, 1),
    "date": date(2030, 1, 1),
    "ISO string with offset": "2030-01-01T01:00:00+01:00",
    "ISO string": "2030-01-01T00:00:00",
    "QDateTime": QDateTime.fromSecsSinceEpoch(int(T), QTimeZone.UTC),
    "float": T,
    "int": int(T),
    "np.int64": np.int64(int(T)),
}


@pytest.mark.parametrize("value", KINDS.values(), ids=KINDS.keys())
def test_scalar(value):
    result = to_epoch_seconds(value)
    assert isinstance(result, float) and result == T


@pytest.mark.parametrize("value", KINDS.values(), ids=KINDS.keys())
def test_sequence_gives_float64_array(value):
    result = to_epoch_seconds([value, value])
    assert result.dtype == np.float64 and result.tolist() == [T, T]


def test_datetime64_scalar_does_not_go_through_nanoseconds():
    assert to_epoch_seconds(np.datetime64("2030-01-01", "s")) == T
    assert to_epoch_seconds(np.datetime64("2030-01-01T00:00:00.5", "ms")) == T + 0.5


def test_datetime64_beyond_the_ns_range():
    assert to_epoch_seconds(np.datetime64("3000-01-01", "D")) == 32503680000.0


@pytest.mark.parametrize("unit", ["ns", "s", "D"])
def test_datetime64_array(unit):
    a = np.array(["2030-01-01", "2030-01-02"], dtype=f"datetime64[{unit}]")
    assert to_epoch_seconds(a).tolist() == [T, T + 86400.0]


def test_object_array_of_dates():
    a = np.array([datetime(2030, 1, 1), "2030-01-02", date(2030, 1, 3)], dtype=object)
    assert to_epoch_seconds(a).tolist() == [T, T + 86400.0, T + 2 * 86400.0]


def test_numeric_array_is_float64_contiguous():
    result = to_epoch_seconds(np.arange(6, dtype=np.int64)[::2])
    assert result.dtype == np.float64 and result.flags.c_contiguous and result.tolist() == [0.0, 2.0, 4.0]


def test_nat_scalar_raises():
    with pytest.raises(ValueError, match="NaT"):
        to_epoch_seconds(np.datetime64("NaT", "ns"))


def test_nat_in_array_raises_unless_gaps_allowed():
    a = np.array(["2030-01-01", "NaT"], dtype="datetime64[s]")
    with pytest.raises(ValueError, match="NaT"):
        to_epoch_seconds(a)
    result = to_epoch_seconds(a, allow_nat=True)
    assert result[0] == T and math.isnan(result[1])


@pytest.mark.parametrize("bad", ["not a date", "2024-13-45", "2024-01-01T", ""])
def test_unparsable_string_raises_value_error_naming_it(bad):
    with pytest.raises(ValueError, match=repr(bad)):
        to_epoch_seconds(bad)


@pytest.mark.parametrize("bad", [object(), None, b"2030-01-01", np.timedelta64(1, "s")],
                         ids=["object", "None", "bytes", "timedelta64"])
def test_unknown_type_raises_type_error(bad):
    with pytest.raises(TypeError, match="expected a number or a date"):
        to_epoch_seconds(bad)

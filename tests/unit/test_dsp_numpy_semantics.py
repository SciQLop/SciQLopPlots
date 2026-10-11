"""DSP functions follow NumPy where NumPy has an answer.

Three departures, fixed together:
- int64 y, NumPy's default integer, raised TypeError.
- A mean or standard deviation with nothing to average gave 0, not NaN.
- spectrogram(overlap=0) meant half a window, so windows that do not overlap
  could not be asked for. The half-window default is now overlap=None.
"""
import numpy as np
import pytest

from SciQLopPlots import dsp

T1 = np.zeros(1)


@pytest.mark.parametrize("dtype", [np.int64, np.int16, np.uint32])
def test_other_integer_types_are_computed_as_float64(dtype):
    x = np.arange(10, dtype=np.float64)
    y = np.arange(10, dtype=dtype)
    _, got = dsp.rolling_mean(x, y, 3)
    _, expected = dsp.rolling_mean(x, y.astype(np.float64), 3)
    assert got.dtype == np.float64
    np.testing.assert_array_equal(got, expected)


def test_rolling_mean_over_only_nan_is_nan():
    x = np.arange(5, dtype=np.float64)
    y = np.array([1.0, np.nan, np.nan, np.nan, 5.0])
    _, out = dsp.rolling_mean(x, y, 1)
    assert np.isnan(out[2])


@pytest.mark.parametrize("window", [1, 3])
def test_rolling_std_with_fewer_than_two_values_is_nan(window):
    x = np.arange(7, dtype=np.float64)
    y = np.array([1.0, 2.0, np.nan, np.nan, np.nan, 4.0, 6.0])
    _, out = dsp.rolling_std(x, y, window)
    assert np.isnan(out[3])


def test_reduce_mean_of_only_nan_is_nan():
    y = np.full((1, 3), np.nan)
    assert np.isnan(dsp.reduce(T1, y, "mean")[1][0])


@pytest.mark.parametrize("axes", [(1,), (0,)], ids=["trailing", "general"])
def test_reduce_axes_mean_of_only_nan_is_nan(axes):
    y = np.array([[1.0, np.nan, np.nan, np.nan]])          # (2, 2): [[1, nan], [nan, nan]]
    out = dsp.reduce_axes(T1, y, (2, 2), axes, op="mean")[1][0]
    with pytest.warns(RuntimeWarning, match="Mean of empty slice"):
        reference = np.nanmean(y.reshape(2, 2), axis=axes[0])
    np.testing.assert_array_equal(out, reference)


@pytest.mark.parametrize("op", ["sum", "norm"])
def test_sum_and_norm_of_only_nan_stay_zero_like_nansum(op):
    y = np.full((1, 3), np.nan)
    assert dsp.reduce(T1, y, op)[1][0] == 0.0


def _n_spectra(**overlap):
    t = np.arange(1024, dtype=np.float64)
    [(st, _, _)] = dsp.spectrogram(t, np.sin(t), window_size=128, **overlap)
    return len(st)


def test_spectrogram_overlap_zero_means_no_overlap():
    assert _n_spectra(overlap=0) == 1024 // 128


@pytest.mark.parametrize("overlap", [{}, {"overlap": None}], ids=["default", "None"])
def test_spectrogram_default_overlap_is_half_a_window(overlap):
    assert _n_spectra(**overlap) == (1024 - 128) // 64 + 1

"""Reductions skip NaN the same way whichever code path runs.

Two bugs: min/max over only NaN returned the accumulator's start value
(±1.8e308), and reduce_axes over non-trailing axes divided a mean by the full
count, NaN included, while the trailing-axes path did not.
"""
import numpy as np
import pytest

from SciQLopPlots import dsp

T1 = np.zeros(1)


@pytest.mark.parametrize("dtype", [np.float64, np.float32])
@pytest.mark.parametrize("op", ["min", "max"])
def test_reduce_extreme_of_only_nan_is_nan(op, dtype):
    y = np.full((1, 3), np.nan, dtype=dtype)
    assert np.isnan(dsp.reduce(T1, y, op)[1][0])
    assert np.isnan(dsp.reduce(T1, y, op, has_gaps=False)[1][0])


@pytest.mark.parametrize("axes", [(1,), (0,)], ids=["trailing", "general"])
@pytest.mark.parametrize("op", ["min", "max"])
def test_reduce_axes_extreme_of_only_nan_is_nan(op, axes):
    y = np.array([[1.0, np.nan, np.nan, np.nan]])          # shape (2, 2): [[1, nan], [nan, nan]]
    out = dsp.reduce_axes(T1, y, (2, 2), axes, op=op)[1][0]
    with pytest.warns(RuntimeWarning, match="All-NaN"):
        reference = {"min": np.nanmin, "max": np.nanmax}[op](y.reshape(2, 2), axis=axes[0])
    np.testing.assert_array_equal(out, reference)


@pytest.mark.parametrize("axes", [(1,), (0,)], ids=["trailing", "general"])
def test_reduce_axes_mean_skips_nan_like_reduce(axes):
    y = np.array([[1.0, np.nan, 3.0, 5.0]])               # (2, 2): [[1, nan], [3, 5]]
    out = dsp.reduce_axes(T1, y, (2, 2), axes, op="mean")[1][0]
    np.testing.assert_allclose(out, np.nanmean(y.reshape(2, 2), axis=axes[0]))

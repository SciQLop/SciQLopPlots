"""Window and reduction names are checked: a typo must not silently pick another method.

They used to fall back without a word: an unknown window meant Hann, an unknown op
meant sum, so reduce(x, y, "average") returned sums.
"""
import numpy as np
import pytest

from SciQLopPlots import dsp

T = np.arange(512, dtype=np.float64)
Y = np.sin(T / 10)
Y3 = np.column_stack([Y, Y, Y])


@pytest.mark.parametrize("call", [
    lambda w: dsp.fft(T, Y, window=w),
    lambda w: dsp.spectrogram(T, Y, window_size=64, window=w),
], ids=["fft", "spectrogram"])
def test_unknown_window_raises(call):
    with pytest.raises(ValueError, match="hann, hamming, blackman, flattop, rectangular"):
        call("hanning")


@pytest.mark.parametrize("call", [
    lambda op: dsp.reduce(T, Y3, op),
    lambda op: dsp.reduce(T, Y3, op, has_gaps=False),
    lambda op: dsp.reduce_axes(T, Y3, (3,), (0,), op=op),
], ids=["reduce", "reduce-no-gaps", "reduce_axes"])
def test_unknown_reduction_raises(call):
    with pytest.raises(ValueError, match="sum, mean, min, max, norm"):
        call("average")


@pytest.mark.parametrize("window", ["hann", "hamming", "blackman", "flattop", "rectangular"])
def test_every_documented_window_is_accepted(window):
    assert len(dsp.fft(T, Y, window=window)) == 1


@pytest.mark.parametrize("op", ["sum", "mean", "min", "max", "norm"])
def test_every_documented_reduction_is_accepted(op):
    _, out = dsp.reduce(T, Y3, op)
    assert out.shape == (len(T),)

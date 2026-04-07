"""SciQLopPlots DSP — gap-aware, SIMD-accelerated, multi-threaded signal processing.

All functions accept numpy arrays and handle data gaps transparently.
Processing runs in C++ with GIL released.
"""
from ._sciqlop_dsp import (
    split_segments,
    interpolate_nan,
    resample,
    fir_filter,
    iir_sos,
    filtfilt,
    sosfiltfilt,
    fft,
    spectrogram,
    rolling_mean,
    rolling_std,
    reduce,
    reduce_axes,
)

__all__ = [
    "split_segments",
    "interpolate_nan",
    "resample",
    "fir_filter",
    "iir_sos",
    "filtfilt",
    "sosfiltfilt",
    "fft",
    "spectrogram",
    "rolling_mean",
    "rolling_std",
    "reduce",
    "reduce_axes",
]

"""Synthetic data generators that mimic SciQLop data providers.

Real SciQLop callbacks: receive (start, stop) epoch floats, return (x, y)
for time series or (x, y, z) for spectrograms.  These fakes produce
deterministic, cheap data so the fuzzer can hammer the pipeline without
network or disk I/O.
"""
from __future__ import annotations

import time
import numpy as np


def make_timeseries(n_components: int = 3, points_per_second: float = 10.0):
    """Return a callback ``(start, stop) -> (x, y)`` for a multi-component line graph."""

    def provider(start: float, stop: float):
        if stop <= start:
            return None
        n = max(int((stop - start) * points_per_second), 2)
        x = np.linspace(start, stop, n, dtype=np.float64)
        cols = [np.sin(x * (i + 1) * 0.1 + i) * (10 ** i) for i in range(n_components)]
        y = np.column_stack(cols).astype(np.float64)
        return x, y

    provider.__name__ = f"timeseries_{n_components}c"
    return provider


def make_spectrogram(n_channels: int = 32, points_per_second: float = 1.0):
    """Return a callback ``(start, stop) -> (x, y, z)`` for a spectrogram."""

    y_axis = np.logspace(0, 4, n_channels, dtype=np.float64)

    def provider(start: float, stop: float):
        if stop <= start:
            return None
        n = max(int((stop - start) * points_per_second), 2)
        x = np.linspace(start, stop, n, dtype=np.float64)
        z = np.random.default_rng(seed=int(start) & 0xFFFF).random(
            (n, n_channels), dtype=np.float64
        )
        return x, y_axis.copy(), z

    provider.__name__ = f"spectrogram_{n_channels}ch"
    return provider


def make_slow_timeseries(
    delay_s: float = 0.05, n_components: int = 3, points_per_second: float = 10.0
):
    """Like make_timeseries but with a configurable sleep to simulate latency."""
    fast = make_timeseries(n_components, points_per_second)

    def provider(start: float, stop: float):
        time.sleep(delay_s)
        return fast(start, stop)

    provider.__name__ = f"slow_timeseries_{delay_s}s"
    return provider


def make_flaky_timeseries(
    failure_rate: float = 0.3, n_components: int = 3, points_per_second: float = 10.0
):
    """Like make_timeseries but randomly returns None to simulate missing data."""
    fast = make_timeseries(n_components, points_per_second)

    def provider(start: float, stop: float):
        if np.random.random() < failure_rate:
            return None
        return fast(start, stop)

    provider.__name__ = f"flaky_timeseries_{failure_rate}"
    return provider


def make_flaky_spectrogram(
    failure_rate: float = 0.3, n_channels: int = 32, points_per_second: float = 1.0
):
    """Like make_spectrogram but randomly returns None."""
    fast = make_spectrogram(n_channels, points_per_second)

    def provider(start: float, stop: float):
        if np.random.random() < failure_rate:
            return None
        return fast(start, stop)

    provider.__name__ = f"flaky_spectrogram_{failure_rate}"
    return provider

"""Shared data generators for demo examples."""
import numpy as np
from datetime import datetime


def sine_multicomponent(n: int = 300_000, time_axis: bool = False):
    """3-component sine wave data (X, Y, Z) at different frequencies."""
    if time_axis:
        start = datetime.now().timestamp()
        x = np.arange(n, dtype=np.float64) + start
    else:
        x = np.arange(n, dtype=np.float64)
    y = np.column_stack([
        np.cos(x / 6) * np.cos(x) * 100,
        np.cos(x / 60) * np.cos(x / 6) * 1300,
        np.cos(x / 600) * np.cos(x / 60) * 17000,
    ])
    return x, y


def butterfly_curve(n: int = 5000):
    """Parametric butterfly curve."""
    t = np.linspace(0, 12 * np.pi, n)
    x = np.sin(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    y = np.cos(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    return x, y


def colormap_signal(nx: int = 300_000, ny: int = 64, time_axis: bool = False):
    """Colormap with vertical modulation, noise, and horizontal sweep."""
    x = np.arange(nx, dtype=np.float64)
    if time_axis:
        x += datetime.now().timestamp()
    y = np.logspace(1, 4, ny)

    v_mod = np.tile(np.cos(np.arange(ny) * 6.28 / (ny * 4)), nx).reshape(nx, ny)
    noise = np.random.rand(nx * ny).reshape(nx, ny)
    h_mod = np.cos(np.arange(nx * ny, dtype=np.float64) * 6.28 / (nx / 3.1)).reshape(nx, ny)
    sig = np.cos(np.arange(nx * ny, dtype=np.float64) * 6.28 / (nx * 10)).reshape(nx, ny)
    z = (v_mod * sig * h_mod + noise)
    z = (z * sig) / (z + 1)
    return x, y, z


def uniform_colormap(nx: int = 200, ny: int = 200):
    """Simple 2D cosine pattern for uniform colormap demos."""
    x = np.arange(nx) * 8.0 / nx - 4.0
    y = np.arange(ny) * 8.0 / ny - 4.0
    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.cos(np.sqrt((xx + 2) ** 2 + yy ** 2))
    return x, y, z


def two_frequency_signal(lower: float, upper: float, freq1: float = 5.0, freq2: float = 13.0):
    """Sum of two sine waves — good for FFT demos."""
    n = max(int((upper - lower) * 100), 64)
    x = np.linspace(lower, upper, n)
    y = np.sin(2 * np.pi * freq1 * x) + 0.5 * np.sin(2 * np.pi * freq2 * x)
    return x, y


def callable_data_producer(generator=sine_multicomponent):
    """Wraps a generator into a callable(start, stop) for lazy-loading demos."""
    def producer(start, stop):
        n = max(int(stop - start), 100)
        return generator(n, time_axis=True)
    return producer

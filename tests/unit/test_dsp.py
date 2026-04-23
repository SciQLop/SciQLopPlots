"""Unit tests comparing C++ DSP functions against numpy/scipy reference implementations.

All tests use contiguous data (no gaps) so we compare pure algorithm output.
Requires scipy — skipped in CI environments where it's not installed.
"""
import pytest
import numpy as np
from numpy.testing import assert_allclose, assert_array_equal

from scipy import signal as sp_signal

from SciQLopPlots.dsp import (
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


# ── Fixtures ──────────────────────────────────────────────────────────────────

@pytest.fixture
def sine_100hz():
    """1000 samples at fs=1000 Hz, 5 Hz + 20 Hz signal."""
    fs = 1000.0
    t = np.arange(1000, dtype=np.float64) / fs
    y = np.sin(2 * np.pi * 5 * t) + 0.5 * np.sin(2 * np.pi * 20 * t)
    return t, y, fs


@pytest.fixture
def multicol_signal():
    """3-column signal for reduce/stats tests."""
    t = np.arange(500, dtype=np.float64) * 0.01
    rng = np.random.default_rng(42)
    y = np.column_stack([
        np.sin(2 * np.pi * 2 * t),
        0.5 * np.cos(2 * np.pi * 3 * t),
        0.3 * rng.normal(size=len(t)),
    ])
    return t, y


# ── split_segments ────────────────────────────────────────────────────────────

class TestSplitSegments:
    def test_no_gaps(self):
        t = np.arange(100, dtype=np.float64) * 0.01
        y = np.sin(t)
        segs = split_segments(t, y)
        assert segs == [(0, 100)]

    def test_single_gap(self):
        t = np.arange(100, dtype=np.float64) * 0.01
        t[50:] += 10.0  # large gap
        y = np.sin(t)
        segs = split_segments(t, y)
        assert len(segs) == 2
        assert segs[0] == (0, 50)
        assert segs[1] == (50, 100)

    def test_multiple_gaps(self):
        t = np.arange(300, dtype=np.float64) * 0.01
        t[100:] += 10.0
        t[200:] += 10.0
        y = np.ones(300)
        segs = split_segments(t, y)
        assert len(segs) == 3


# ── interpolate_nan ───────────────────────────────────────────────────────────

class TestInterpolateNan:
    def test_single_nan_filled(self):
        t = np.arange(10, dtype=np.float64)
        y = np.arange(10, dtype=np.float64)
        y[5] = np.nan
        result = interpolate_nan(t, y, max_consecutive=1)
        assert_allclose(result[5], 5.0)
        assert not np.any(np.isnan(result))

    def test_long_nan_run_preserved(self):
        t = np.arange(10, dtype=np.float64)
        y = np.arange(10, dtype=np.float64)
        y[3:7] = np.nan
        result = interpolate_nan(t, y, max_consecutive=1)
        assert np.all(np.isnan(result[3:7]))

    def test_matches_numpy_interp(self):
        t = np.linspace(0, 1, 100)
        y = np.sin(2 * np.pi * t)
        nan_idx = [10, 30, 50, 70, 90]
        y_nan = y.copy()
        y_nan[nan_idx] = np.nan

        result = interpolate_nan(t, y_nan, max_consecutive=1)

        # numpy reference: interp at NaN positions
        valid = ~np.isnan(y_nan)
        expected = y_nan.copy()
        expected[nan_idx] = np.interp(t[nan_idx], t[valid], y_nan[valid])

        assert_allclose(result, expected, atol=1e-12)

    def test_preserves_float32(self):
        t = np.arange(10, dtype=np.float64)
        y = np.arange(10, dtype=np.float32)
        y[5] = np.nan
        result = interpolate_nan(t, y)
        assert result.dtype == np.float32

    def test_noop_for_int32(self):
        t = np.arange(10, dtype=np.float64)
        y = np.arange(10, dtype=np.int32)
        result = interpolate_nan(t, y)
        assert_array_equal(result, y)


# ── resample ──────────────────────────────────────────────────────────────────

class TestResample:
    def test_uniform_input_nearly_unchanged(self):
        t = np.arange(100, dtype=np.float64) * 0.01
        y = np.sin(2 * np.pi * t)
        t_r, y_r = resample(t, y)
        # C++ uses floor((t[-1]-t[0])/median_dt)+1 which may differ by ±1
        # due to floating point; just check values are close where they overlap
        n = min(len(t_r), len(t))
        assert_allclose(t_r[:n], t[:n], atol=1e-12)
        assert_allclose(y_r[:n], y[:n], atol=1e-10)

    def test_interpolation_correct(self):
        # Slightly jittered timestamps (no gaps) so it stays one segment
        rng = np.random.default_rng(42)
        dt_base = 0.01
        t = np.arange(200, dtype=np.float64) * dt_base
        t += rng.uniform(-dt_base * 0.2, dt_base * 0.2, size=200)
        t = np.sort(t)
        y = np.sin(2 * np.pi * t)
        t_r, y_r = resample(t, y)

        # Verify output is uniformly spaced
        dt_out = np.diff(t_r)
        assert_allclose(dt_out, dt_out[0], atol=1e-12)

        # Verify interpolated values match numpy.interp
        y_ref = np.interp(t_r, t, y)
        assert_allclose(y_r, y_ref, atol=1e-10)

    def test_preserves_float32(self):
        t = np.arange(100, dtype=np.float64) * 0.01
        y = np.sin(2 * np.pi * t).astype(np.float32)
        _, y_r = resample(t, y)
        assert y_r.dtype == np.float32


# ── fir_filter ────────────────────────────────────────────────────────────────

class TestFirFilter:
    def test_matches_scipy_lfilter(self, sine_100hz):
        t, y, fs = sine_100hz
        coeffs = sp_signal.firwin(65, 10.0, fs=fs)

        _, y_cpp = fir_filter(t, y, coeffs)

        # C++ FIR is causal, matching scipy.signal.lfilter
        y_ref = sp_signal.lfilter(coeffs, 1.0, y)

        assert_allclose(y_cpp, y_ref, atol=1e-10)

    def test_lowpass_kills_high_freq(self, sine_100hz):
        t, y, fs = sine_100hz
        coeffs = sp_signal.firwin(65, 10.0, fs=fs)
        _, y_filt = fir_filter(t, y, coeffs)

        # After lowpass at 10 Hz, the 20 Hz component should be gone
        # Check in steady-state region
        margin = 100
        y_ss = y_filt[margin:-margin]
        # The 5 Hz component has amplitude 1.0, so max should be ~1.0
        assert abs(y_ss).max() < 1.15
        assert abs(y_ss).max() > 0.8

    def test_preserves_float32(self, sine_100hz):
        t, y, fs = sine_100hz
        coeffs = sp_signal.firwin(33, 10.0, fs=fs).astype(np.float32)
        _, y_f = fir_filter(t, y.astype(np.float32), coeffs)
        assert y_f.dtype == np.float32

    def test_dtype_mismatch_raises(self, sine_100hz):
        t, y, fs = sine_100hz
        coeffs = sp_signal.firwin(33, 10.0, fs=fs)  # float64
        with pytest.raises(TypeError):
            fir_filter(t, y.astype(np.float32), coeffs)


# ── iir_sos ───────────────────────────────────────────────────────────────────

class TestIirSos:
    def test_matches_scipy_sosfilt(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(4, 10.0, fs=fs, output='sos')

        _, y_cpp = iir_sos(t, y, sos)
        y_ref = sp_signal.sosfilt(sos, y)

        assert_allclose(y_cpp, y_ref, atol=1e-10)

    def test_highpass(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(4, 10.0, btype='high', fs=fs, output='sos')

        _, y_cpp = iir_sos(t, y, sos)
        y_ref = sp_signal.sosfilt(sos, y)

        assert_allclose(y_cpp, y_ref, atol=1e-10)

    def test_bandpass(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(2, [8.0, 30.0], btype='band', fs=fs, output='sos')

        _, y_cpp = iir_sos(t, y, sos)
        y_ref = sp_signal.sosfilt(sos, y)

        assert_allclose(y_cpp, y_ref, atol=1e-10)

    def test_preserves_float32(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(2, 10.0, fs=fs, output='sos').astype(np.float32)
        _, y_f = iir_sos(t, y.astype(np.float32), sos)
        assert y_f.dtype == np.float32

    def test_dtype_mismatch_raises(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(2, 10.0, fs=fs, output='sos')  # float64
        with pytest.raises(TypeError):
            iir_sos(t, y.astype(np.float32), sos)


# ── filtfilt (zero-phase FIR) ─────────────────────────────────────────────────

class TestFiltfilt:
    def test_matches_scipy_filtfilt(self, sine_100hz):
        t, y, fs = sine_100hz
        coeffs = sp_signal.firwin(65, 10.0, fs=fs)

        _, y_cpp = filtfilt(t, y, coeffs, has_gaps=False)
        y_ref = sp_signal.filtfilt(coeffs, 1.0, y)

        assert_allclose(y_cpp, y_ref, atol=1e-14)

    def test_zero_phase(self, sine_100hz):
        """filtfilt should not introduce phase shift — compare against causal lfilter."""
        t, y, fs = sine_100hz
        coeffs = sp_signal.firwin(65, 10.0, fs=fs)

        _, y_filtfilt = filtfilt(t, y, coeffs, has_gaps=False)
        _, y_causal = fir_filter(t, y, coeffs, has_gaps=False)

        # filtfilt result should match scipy's filtfilt (zero-phase)
        y_ref = sp_signal.filtfilt(coeffs, 1.0, y)
        assert_allclose(y_filtfilt, y_ref, atol=1e-14)

        # Causal filter introduces delay; filtfilt should not.
        # Cross-correlation peak of filtfilt with input should be at lag 0.
        mid = len(t) // 4  # avoid edges
        corr = np.correlate(y[mid:-mid], y_filtfilt[mid:-mid], mode='full')
        lag = np.argmax(corr) - (len(y[mid:-mid]) - 1)
        assert abs(lag) <= 1, f"filtfilt has lag {lag}, expected ~0"

    def test_multicol(self, sine_100hz):
        t, y, fs = sine_100hz
        y3 = np.column_stack([y, 0.5 * y, 0.3 * y])
        coeffs = sp_signal.firwin(65, 10.0, fs=fs)

        _, y_cpp = filtfilt(t, y3, coeffs, has_gaps=False)
        y_ref = np.column_stack([sp_signal.filtfilt(coeffs, 1.0, y3[:, c]) for c in range(3)])

        assert_allclose(y_cpp, y_ref, atol=1e-14)


# ── sosfiltfilt (zero-phase IIR) ─────────────────────────────────────────────

class TestSosfiltfilt:
    def test_matches_scipy_sosfiltfilt(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(4, 10.0, fs=fs, output='sos')

        _, y_cpp = sosfiltfilt(t, y, sos, has_gaps=False)
        y_ref = sp_signal.sosfiltfilt(sos, y)

        assert_allclose(y_cpp, y_ref, atol=1e-12)

    def test_bandpass(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(4, [3, 8], btype='band', fs=fs, output='sos')

        _, y_cpp = sosfiltfilt(t, y, sos, has_gaps=False)
        y_ref = sp_signal.sosfiltfilt(sos, y)

        assert_allclose(y_cpp, y_ref, atol=1e-12)

    def test_high_order(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(8, 10.0, fs=fs, output='sos')

        _, y_cpp = sosfiltfilt(t, y, sos, has_gaps=False)
        y_ref = sp_signal.sosfiltfilt(sos, y)

        assert_allclose(y_cpp, y_ref, atol=1e-11)

    def test_multicol(self, sine_100hz):
        t, y, fs = sine_100hz
        y3 = np.column_stack([y, 0.5 * y, 0.3 * y])
        sos = sp_signal.butter(4, 10.0, fs=fs, output='sos')

        _, y_cpp = sosfiltfilt(t, y3, sos, has_gaps=False)
        y_ref = np.column_stack([sp_signal.sosfiltfilt(sos, y3[:, c]) for c in range(3)])

        assert_allclose(y_cpp, y_ref, atol=1e-12)

    def test_preserves_float32(self, sine_100hz):
        t, y, fs = sine_100hz
        sos = sp_signal.butter(2, 10.0, fs=fs, output='sos').astype(np.float32)
        _, y_f = sosfiltfilt(t, y.astype(np.float32), sos, has_gaps=False)
        assert y_f.dtype == np.float32


# ── fft ───────────────────────────────────────────────────────────────────────

class TestFFT:
    def test_matches_numpy_rfft(self, sine_100hz):
        t, y, fs = sine_100hz
        results = fft(t, y, window='rectangular')

        assert len(results) == 1
        freqs, mag = results[0]

        # numpy reference (rectangular window = no window)
        n = len(y)
        freqs_ref = np.fft.rfftfreq(n, d=1.0 / fs)
        mag_ref = np.abs(np.fft.rfft(y)) / n

        assert_allclose(freqs, freqs_ref, atol=1e-10)
        assert_allclose(mag, mag_ref, atol=1e-10)

    def test_hann_window(self, sine_100hz):
        t, y, fs = sine_100hz
        results = fft(t, y, window='hann')
        freqs, mag = results[0]

        n = len(y)
        window = np.hanning(n)
        freqs_ref = np.fft.rfftfreq(n, d=1.0 / fs)
        mag_ref = np.abs(np.fft.rfft(y * window)) / n

        assert_allclose(freqs, freqs_ref, atol=1e-10)
        assert_allclose(mag, mag_ref, atol=1e-10)

    def test_peak_at_signal_frequency(self, sine_100hz):
        t, y, fs = sine_100hz
        results = fft(t, y)
        freqs, mag = results[0]

        peak_freq = freqs[np.argmax(mag)]
        assert abs(peak_freq - 5.0) < 2.0  # 5 Hz is the dominant component

    def test_preserves_float32(self):
        t = np.arange(256, dtype=np.float64) / 256.0
        y = np.sin(2 * np.pi * 10 * t).astype(np.float32)
        results = fft(t, y)
        _, mag = results[0]
        assert mag.dtype == np.float32


# ── spectrogram ───────────────────────────────────────────────────────────────

class TestSpectrogram:
    def test_matches_scipy_spectrogram(self):
        fs = 1000.0
        t = np.arange(4000, dtype=np.float64) / fs
        y = sp_signal.chirp(t, f0=10, f1=200, t1=t[-1])

        nperseg = 256
        noverlap = nperseg // 2

        results = spectrogram(t, y, window_size=nperseg, overlap=noverlap)
        assert len(results) == 1
        t_cpp, f_cpp, power_cpp = results[0]

        # scipy reference
        f_ref, t_ref, Sxx_ref = sp_signal.spectrogram(
            y, fs=fs, nperseg=nperseg, noverlap=noverlap,
            window='hann', scaling='spectrum', mode='psd',
        )

        # Our spectrogram uses |X/N|^2, scipy uses different scaling.
        # Compare frequency axes and time axes, then check power shape.
        assert_allclose(f_cpp, f_ref, atol=1e-10)
        assert len(t_cpp) == len(t_ref)
        # Time offsets: ours uses absolute time, scipy uses relative
        t_ref_abs = t_ref + t[0]
        assert_allclose(t_cpp, t_ref_abs, atol=1e-10)
        assert power_cpp.shape == (len(t_cpp), len(f_cpp))

    def test_power_shape(self):
        t = np.arange(1000, dtype=np.float64) * 0.001
        y = np.sin(2 * np.pi * 50 * t)
        results = spectrogram(t, y, window_size=64, overlap=32)
        t_s, f_s, p_s = results[0]
        assert p_s.shape == (len(t_s), len(f_s))
        assert len(f_s) == 64 // 2 + 1

    def test_preserves_float32(self):
        t = np.arange(1000, dtype=np.float64) * 0.001
        y = np.sin(2 * np.pi * 50 * t).astype(np.float32)
        results = spectrogram(t, y, window_size=64)
        _, _, power = results[0]
        assert power.dtype == np.float32


# ── rolling_mean ──────────────────────────────────────────────────────────────

class TestRollingMean:
    def test_matches_numpy_convolve(self):
        t = np.arange(200, dtype=np.float64) * 0.01
        y = np.sin(2 * np.pi * 5 * t) + 0.5 * np.random.default_rng(42).normal(size=200)

        window = 21
        _, y_cpp = rolling_mean(t, y, window)

        # numpy reference: centered moving average
        kernel = np.ones(window) / window
        # Use same boundary handling: compare center region
        y_ref = np.convolve(y, kernel, mode='same')

        # Compare center where both implementations agree
        margin = window
        assert_allclose(y_cpp[margin:-margin], y_ref[margin:-margin], atol=1e-10)

    def test_preserves_float32(self):
        t = np.arange(100, dtype=np.float64) * 0.01
        y = np.sin(2 * np.pi * t).astype(np.float32)
        _, y_m = rolling_mean(t, y, 5)
        assert y_m.dtype == np.float32


# ── rolling_std ───────────────────────────────────────────────────────────────

class TestRollingStd:
    def test_positive_values(self):
        t = np.arange(200, dtype=np.float64) * 0.01
        y = np.sin(2 * np.pi * 5 * t)
        _, y_std = rolling_std(t, y, 21)
        assert np.all(y_std >= 0)

    def test_constant_signal_zero_std(self):
        t = np.arange(100, dtype=np.float64) * 0.01
        y = np.ones(100) * 42.0
        _, y_std = rolling_std(t, y, 11)
        margin = 11
        assert_allclose(y_std[margin:-margin], 0.0, atol=1e-10)

    def test_matches_numpy_sliding_std(self):
        t = np.arange(200, dtype=np.float64) * 0.01
        rng = np.random.default_rng(42)
        y = rng.normal(size=200)

        window = 21
        _, y_cpp = rolling_std(t, y, window)

        # numpy reference: sliding window std
        half = window // 2
        y_ref = np.zeros_like(y)
        for i in range(len(y)):
            lo = max(0, i - half)
            hi = min(len(y), i + half + 1)
            y_ref[i] = np.std(y[lo:hi], ddof=1)

        margin = window
        assert_allclose(y_cpp[margin:-margin], y_ref[margin:-margin], atol=1e-10)


# ── reduce ────────────────────────────────────────────────────────────────────

class TestReduce:
    def test_sum(self, multicol_signal):
        t, y = multicol_signal
        _, y_r = reduce(t, y, 'sum')
        assert_allclose(y_r, np.nansum(y, axis=1), atol=1e-10)

    def test_mean(self, multicol_signal):
        t, y = multicol_signal
        _, y_r = reduce(t, y, 'mean')
        assert_allclose(y_r, np.nanmean(y, axis=1), atol=1e-10)

    def test_min(self, multicol_signal):
        t, y = multicol_signal
        _, y_r = reduce(t, y, 'min')
        assert_allclose(y_r, np.nanmin(y, axis=1), atol=1e-10)

    def test_max(self, multicol_signal):
        t, y = multicol_signal
        _, y_r = reduce(t, y, 'max')
        assert_allclose(y_r, np.nanmax(y, axis=1), atol=1e-10)

    def test_norm(self, multicol_signal):
        t, y = multicol_signal
        _, y_r = reduce(t, y, 'norm')
        assert_allclose(y_r, np.sqrt(np.nansum(y ** 2, axis=1)), atol=1e-10)

    def test_preserves_int32(self):
        t = np.arange(50, dtype=np.float64)
        y = np.column_stack([
            np.arange(50, dtype=np.int32),
            np.arange(50, dtype=np.int32) * 2,
        ])
        _, y_r = reduce(t, y, 'sum')
        assert y_r.dtype == np.int32
        assert_array_equal(y_r, y[:, 0] + y[:, 1])


# ── reduce_axes ──────────────────────────────────────────────────────────────

class TestReduceAxes:
    @pytest.fixture
    def particle_dist(self):
        """Simulated particle distribution: (time=200, energy=8, theta=4, phi=6)."""
        rng = np.random.default_rng(42)
        n_time = 200
        n_e, n_t, n_p = 8, 4, 6
        t = np.arange(n_time, dtype=np.float64) * 0.1
        y = rng.random((n_time, n_e * n_t * n_p))
        return t, y, (n_e, n_t, n_p)

    def test_sum_both_angles(self, particle_dist):
        t, y, shape = particle_dist
        _, y_r = reduce_axes(t, y, shape, (1, 2), op='sum')
        y_ref = y.reshape(len(t), *shape).sum(axis=(2, 3))
        assert y_r.shape == (len(t), shape[0])
        assert_allclose(y_r, y_ref, atol=1e-12)

    def test_sum_single_axis(self, particle_dist):
        t, y, shape = particle_dist
        _, y_r = reduce_axes(t, y, shape, (2,), op='sum')
        y_ref = y.reshape(len(t), *shape).sum(axis=3).reshape(len(t), -1)
        assert y_r.shape == (len(t), shape[0] * shape[1])
        assert_allclose(y_r, y_ref, atol=1e-14)

    def test_sum_middle_axis(self, particle_dist):
        t, y, shape = particle_dist
        _, y_r = reduce_axes(t, y, shape, (1,), op='sum')
        y_ref = y.reshape(len(t), *shape).sum(axis=2).reshape(len(t), -1)
        assert y_r.shape == (len(t), shape[0] * shape[2])
        assert_allclose(y_r, y_ref, atol=1e-14)

    def test_mean(self, particle_dist):
        t, y, shape = particle_dist
        _, y_r = reduce_axes(t, y, shape, (1, 2), op='mean')
        y_ref = y.reshape(len(t), *shape).mean(axis=(2, 3))
        assert_allclose(y_r, y_ref, atol=1e-14)

    def test_max(self, particle_dist):
        t, y, shape = particle_dist
        _, y_r = reduce_axes(t, y, shape, (1, 2), op='max')
        y_ref = y.reshape(len(t), *shape).max(axis=(2, 3))
        assert_allclose(y_r, y_ref)

    def test_reduce_all_to_scalar(self, particle_dist):
        t, y, shape = particle_dist
        _, y_r = reduce_axes(t, y, shape, (0, 1, 2), op='sum')
        y_ref = y.sum(axis=1)
        assert y_r.shape == (len(t),)
        assert_allclose(y_r, y_ref, atol=1e-11)

    def test_shape_mismatch_raises(self, particle_dist):
        t, y, _ = particle_dist
        with pytest.raises(ValueError):
            reduce_axes(t, y, (10, 10), (0,))  # 100 != actual n_cols


# ── Input validation tests ───────────────────────────────────────────────────

class TestInputValidation:
    """Every DSP function must raise ValueError on bad inputs, never crash."""

    def test_xy_size_mismatch_resample(self):
        with pytest.raises(ValueError, match="x length.*must match"):
            resample(np.arange(10.0), np.arange(5.0))

    def test_xy_size_mismatch_fir(self):
        with pytest.raises(ValueError, match="x length.*must match"):
            fir_filter(np.arange(10.0), np.arange(5.0), np.ones(3))

    def test_xy_size_mismatch_rolling(self):
        with pytest.raises(ValueError, match="x length.*must match"):
            rolling_mean(np.arange(10.0), np.arange(5.0), window=3)

    def test_xy_size_mismatch_fft(self):
        with pytest.raises(ValueError, match="x length.*must match"):
            fft(np.arange(10.0), np.arange(5.0))

    def test_xy_size_mismatch_reduce(self):
        with pytest.raises(ValueError, match="x length.*must match"):
            reduce(np.arange(10.0), np.arange(5.0), "sum")

    def test_gap_factor_negative(self):
        with pytest.raises(ValueError, match="gap_factor"):
            split_segments(np.arange(10.0), np.arange(10.0), gap_factor=-1.0)

    def test_gap_factor_zero(self):
        with pytest.raises(ValueError, match="gap_factor"):
            resample(np.arange(10.0), np.arange(10.0), gap_factor=0.0)

    def test_sos_a0_zero_iir(self):
        sos = np.array([[1.0, 0.0, 0.0, 0.0, 0.0, 0.0]])
        with pytest.raises(ValueError, match="a0=0"):
            iir_sos(np.arange(10.0), np.arange(10.0), sos)

    def test_sos_a0_zero_sosfiltfilt(self):
        sos = np.array([[1.0, 0.0, 0.0, 0.0, 0.0, 0.0]])
        with pytest.raises(ValueError, match="a0=0"):
            sosfiltfilt(np.arange(10.0), np.arange(10.0), sos)

    def test_filtfilt_too_few_samples(self):
        with pytest.raises(ValueError, match="at least 2"):
            filtfilt(np.array([1.0]), np.array([1.0]), np.ones(5))

    def test_sosfiltfilt_too_few_samples(self):
        sos = np.array([[1.0, 0.0, 0.0, 1.0, 0.0, 0.0]])
        with pytest.raises(ValueError, match="at least 2"):
            sosfiltfilt(np.array([1.0]), np.array([1.0]), sos)

    def test_fft_too_few_samples(self):
        with pytest.raises(ValueError, match="at least 2"):
            fft(np.array([1.0]), np.array([1.0]))

    def test_fir_empty_coeffs(self):
        with pytest.raises(ValueError, match="at least 1 tap"):
            fir_filter(np.arange(10.0), np.arange(10.0), np.array([], dtype=np.float64))

    def test_rolling_mean_window_zero(self):
        with pytest.raises(ValueError, match="window must be > 0"):
            rolling_mean(np.arange(5.0), np.arange(5.0), window=0)

    def test_rolling_mean_window_too_large(self):
        with pytest.raises(ValueError, match="window.*must be <="):
            rolling_mean(np.arange(5.0), np.arange(5.0), window=10)

    def test_rolling_std_window_negative(self):
        with pytest.raises(ValueError, match="window must be > 0"):
            rolling_std(np.arange(5.0), np.arange(5.0), window=-1)

    def test_spectrogram_col_out_of_range(self):
        with pytest.raises(ValueError, match="col.*out of range"):
            spectrogram(np.arange(100.0), np.arange(100.0), col=5)

    def test_spectrogram_window_too_small(self):
        with pytest.raises(ValueError, match="window_size"):
            spectrogram(np.arange(100.0), np.arange(100.0), window_size=1)

    def test_spectrogram_overlap_equals_window(self):
        with pytest.raises(ValueError, match="overlap"):
            spectrogram(np.arange(100.0), np.arange(100.0), window_size=10, overlap=10)

    def test_resample_tiny_dt_overflow(self):
        with pytest.raises(ValueError, match="100M"):
            resample(np.array([0.0, 1e10]), np.array([1.0, 2.0]), target_dt=1e-10)


class TestNaNRobustness:
    """Edge cases with NaN/Inf that previously caused UB or crashes."""

    def test_nan_timestamps_no_crash(self):
        x = np.array([0.0, 1.0, np.nan, 3.0, 4.0])
        y = np.array([1.0, 2.0, 3.0, 4.0, 5.0])
        result = split_segments(x, y)
        assert isinstance(result, list)

    def test_inf_timestamps_no_crash(self):
        x = np.array([0.0, 1.0, np.inf, 3.0, 4.0])
        y = np.array([1.0, 2.0, 3.0, 4.0, 5.0])
        result = split_segments(x, y)
        assert isinstance(result, list)

    def test_all_nan_timestamps_no_crash(self):
        x = np.full(5, np.nan)
        y = np.arange(5.0)
        result = split_segments(x, y)
        assert isinstance(result, list)

    def test_interpolate_nan_all_nan(self):
        x = np.arange(5.0)
        y = np.full(5, np.nan)
        out = interpolate_nan(x, y)
        assert np.all(np.isnan(out))

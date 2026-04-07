"""DSP Gallery — showcases gap-aware C++ DSP operations on scientific time series.

Demonstrates: gap detection, NaN interpolation, uniform resampling,
FIR/IIR filtering, FFT, spectrogram, rolling stats, dimension reduction.

All DSP processing runs in C++ (SIMD + thread pool) via SciQLopPlots.dsp.
"""
import sys
import numpy as np
from scipy import signal as sp_signal  # chirp generation + filter design
from PySide6.QtWidgets import QApplication, QMainWindow, QTabWidget
from PySide6.QtGui import QColorConstants

from SciQLopPlots import (
    SciQLopPlot, SciQLopMultiPlotPanel,
    PlotType, GraphType,
)
from SciQLopPlots.dsp import (
    split_segments, interpolate_nan, resample,
    fir_filter, iir_sos, fft, spectrogram,
    rolling_mean, rolling_std, reduce,
)


# ── Synthetic data with gaps and NaN ────────────────────────────────────────

def gapped_signal(n=2000, n_gaps=3, gap_size=50, rng_seed=42):
    """Multi-component signal with data gaps and isolated NaN values."""
    rng = np.random.default_rng(rng_seed)
    dt = 0.01
    t = np.arange(n, dtype=np.float64) * dt

    y0 = np.sin(2 * np.pi * 2.0 * t)
    y1 = 0.5 * np.sin(2 * np.pi * 7.0 * t)
    y2 = 0.3 * np.sin(2 * np.pi * 15.0 * t) + 0.1 * rng.normal(size=n)
    y = np.column_stack([y0 + y1 + y2, y0, y1])

    gap_starts = sorted(rng.choice(range(200, n - 200), size=n_gaps, replace=False))
    for gs in gap_starts:
        t[gs:] += gap_size * dt

    nan_indices = rng.choice(range(10, n - 10), size=20, replace=False)
    y[nan_indices, 0] = np.nan

    return t, y


def chirp_signal(n=4000):
    """Chirp signal (frequency sweep) for spectrogram demo."""
    dt = 0.001
    t = np.arange(n, dtype=np.float64) * dt
    y = sp_signal.chirp(t, f0=10, f1=200, t1=t[-1], method='linear')
    gap_start = n // 2
    t[gap_start:] += 0.5
    return t, y


# ── Tab builders ────────────────────────────────────────────────────────────

def create_gap_detection_tab():
    """Shows original gapped signal with segment boundaries detected by C++ DSP."""
    t, y = gapped_signal()
    col = y[:, 0].copy()

    # C++ gap detection
    segments = split_segments(t, col)

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    def raw_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], col[mask].reshape(-1, 1)

    panel.plot(raw_provider, labels=["Raw (with gaps)"],
               colors=[QColorConstants.DarkBlue], plot_type=PlotType.TimeSeries)

    # Segment ID indicator
    seg_y = np.zeros_like(t)
    for seg_idx, (s, e) in enumerate(segments):
        seg_y[s:e] = seg_idx

    def seg_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], seg_y[mask].reshape(-1, 1)

    panel.plot(seg_provider, labels=["Segment ID"],
               colors=[QColorConstants.DarkRed], plot_type=PlotType.TimeSeries)

    return panel


def create_nan_interpolation_tab():
    """Shows NaN filling: before and after, using C++ DSP."""
    t, y = gapped_signal()
    col = y[:, 0].copy()

    # C++ NaN interpolation
    col_fixed = interpolate_nan(t, col, max_consecutive=1)

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    def before_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], col[mask].reshape(-1, 1)

    def after_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], col_fixed[mask].reshape(-1, 1)

    panel.plot(before_provider, labels=["Before (with NaN)"],
               colors=[QColorConstants.Red], plot_type=PlotType.TimeSeries)
    panel.plot(after_provider, labels=["After (NaN interpolated)"],
               colors=[QColorConstants.DarkGreen], plot_type=PlotType.TimeSeries)

    return panel


def create_resampling_tab():
    """Shows irregular → uniform resampling per segment, using C++ DSP."""
    t, y = gapped_signal()

    # C++ gap-aware resampling (interpolate NaN first)
    col = interpolate_nan(t, y[:, 0])
    t_res, y_res = resample(t, col)

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    def orig_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], y[mask, 0:1]

    panel.plot(orig_provider, labels=["Original (irregular)"],
               colors=[QColorConstants.DarkBlue], plot_type=PlotType.TimeSeries)

    def resampled_provider(start, stop):
        mask = (t_res >= start) & (t_res <= stop)
        return t_res[mask], y_res[mask].reshape(-1, 1)

    panel.plot(resampled_provider, labels=["Resampled (uniform)"],
               colors=[QColorConstants.DarkGreen], plot_type=PlotType.TimeSeries)

    return panel


def create_filter_tab():
    """Shows FIR lowpass and IIR highpass filtering, using C++ DSP."""
    t, y = gapped_signal()
    col = interpolate_nan(t, y[:, 0])

    # Design filters in Python (scipy), apply in C++
    fs = 100.0  # dt=0.01 → fs=100 Hz
    fir_coeffs = sp_signal.firwin(65, 5.0, fs=fs)
    sos = sp_signal.butter(4, 5.0, btype='high', fs=fs, output='sos')

    t_fir, y_fir = fir_filter(t, col, fir_coeffs)
    t_iir, y_iir = iir_sos(t, col, sos)

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    def orig_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], col[mask].reshape(-1, 1)

    def fir_provider(start, stop):
        mask = (t_fir >= start) & (t_fir <= stop)
        return t_fir[mask], y_fir[mask].reshape(-1, 1)

    def iir_provider(start, stop):
        mask = (t_iir >= start) & (t_iir <= stop)
        return t_iir[mask], y_iir[mask].reshape(-1, 1)

    panel.plot(orig_provider, labels=["Original"],
               colors=[QColorConstants.DarkBlue], plot_type=PlotType.TimeSeries)
    panel.plot(fir_provider, labels=["FIR Lowpass 5 Hz"],
               colors=[QColorConstants.DarkGreen], plot_type=PlotType.TimeSeries)
    panel.plot(iir_provider, labels=["IIR Highpass 5 Hz"],
               colors=[QColorConstants.DarkRed], plot_type=PlotType.TimeSeries)

    return panel


def create_fft_tab():
    """Shows per-segment FFT magnitude spectrum, using C++ DSP."""
    t, y = gapped_signal()
    col = interpolate_nan(t, y[:, 0])

    # C++ gap-aware FFT (returns list of (freqs, magnitude) per segment)
    fft_results = fft(t, col)

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    def time_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], col[mask].reshape(-1, 1)

    panel.plot(time_provider, labels=["Signal"],
               colors=[QColorConstants.DarkBlue], plot_type=PlotType.TimeSeries)

    # Plot FFT of the largest segment
    largest = max(fft_results, key=lambda r: len(r[0]))
    freqs, magnitude = largest

    plot = SciQLopPlot()
    plot.plot(freqs, magnitude.reshape(-1, 1),
              labels=["FFT magnitude"], colors=[QColorConstants.DarkMagenta])
    plot.x_axis().set_range(0, float(freqs[-1]))
    plot.y_axis().set_range(0, float(magnitude.max() * 1.1))
    panel.add_plot(plot)

    return panel


def create_spectrogram_tab():
    """Shows a chirp signal and its spectrogram per segment, using C++ DSP."""
    t, y = chirp_signal()

    # C++ gap-aware spectrogram (returns list of (t, f, power) per segment)
    spec_results = spectrogram(t, y, window_size=256)

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    # Time-domain chirp (reassemble with NaN at gaps)
    t_res, y_res = resample(t, y)

    def chirp_provider(start, stop):
        mask = (t_res >= start) & (t_res <= stop)
        return t_res[mask], y_res[mask].reshape(-1, 1)

    panel.plot(chirp_provider, labels=["Chirp"],
               colors=[QColorConstants.DarkBlue], plot_type=PlotType.TimeSeries)

    # Concatenate spectrograms from all segments with NaN gap columns
    t_parts, power_parts = [], []
    freqs = None
    for t_spec, f_spec, power in spec_results:
        if len(t_spec) == 0:
            continue
        freqs = f_spec
        t_parts.append(t_spec)
        power_parts.append(power)

    if freqs is not None and t_parts:
        t_all, power_all = [], []
        for i, (tp, pp) in enumerate(zip(t_parts, power_parts)):
            if i > 0:
                gap_t = (t_all[-1][-1] + tp[0]) / 2.0
                t_all.append(np.array([gap_t]))
                power_all.append(np.full((1, len(freqs)), np.nan))
            t_all.append(tp)
            power_all.append(pp)

        t_cat = np.concatenate(t_all)
        power_cat = np.concatenate(power_all, axis=0)
        power_db = 10 * np.log10(power_cat + 1e-12)

        plot = SciQLopPlot()
        plot.plot(t_cat, freqs, power_db)
        plot.x_axis().set_range(float(t_cat[0]), float(t_cat[-1]))
        plot.y_axis().set_range(float(freqs[0]), float(freqs[-1]))
        panel.add_plot(plot)

    return panel


def create_rolling_stats_tab():
    """Shows rolling mean and rolling std, using C++ DSP."""
    t, y = gapped_signal()
    col = interpolate_nan(t, y[:, 0])
    window = 51

    # C++ gap-aware rolling stats
    t_mean, y_mean = rolling_mean(t, col, window)
    t_std, y_std = rolling_std(t, col, window)

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    def orig_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], col[mask].reshape(-1, 1)

    def mean_provider(start, stop):
        mask = (t_mean >= start) & (t_mean <= stop)
        return t_mean[mask], y_mean[mask].reshape(-1, 1)

    def std_provider(start, stop):
        mask = (t_std >= start) & (t_std <= stop)
        return t_std[mask], y_std[mask].reshape(-1, 1)

    panel.plot(orig_provider, labels=["Original"],
               colors=[QColorConstants.DarkBlue], plot_type=PlotType.TimeSeries)
    panel.plot(mean_provider, labels=[f"Rolling Mean (w={window})"],
               colors=[QColorConstants.DarkGreen], plot_type=PlotType.TimeSeries)
    panel.plot(std_provider, labels=[f"Rolling Std (w={window})"],
               colors=[QColorConstants.DarkRed], plot_type=PlotType.TimeSeries)

    return panel


def create_reduce_tab():
    """Shows dimension reduction: 3-component signal → norm, sum, mean, using C++ DSP."""
    t, y = gapped_signal()

    # C++ gap-aware reduction
    t_norm, y_norm = reduce(t, y, "norm")
    t_sum, y_sum = reduce(t, y, "sum")
    t_mean, y_mean_r = reduce(t, y, "mean")

    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    def components_provider(start, stop):
        mask = (t >= start) & (t <= stop)
        return t[mask], y[mask]

    def norm_provider(start, stop):
        mask = (t_norm >= start) & (t_norm <= stop)
        return t_norm[mask], y_norm[mask].reshape(-1, 1)

    def sum_provider(start, stop):
        mask = (t_sum >= start) & (t_sum <= stop)
        return t_sum[mask], y_sum[mask].reshape(-1, 1)

    def mean_provider(start, stop):
        mask = (t_mean >= start) & (t_mean <= stop)
        return t_mean[mask], y_mean_r[mask].reshape(-1, 1)

    panel.plot(components_provider, labels=["X", "Y", "Z"],
               colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
               plot_type=PlotType.TimeSeries)
    panel.plot(norm_provider, labels=["L2 Norm"],
               colors=[QColorConstants.DarkMagenta], plot_type=PlotType.TimeSeries)
    panel.plot(sum_provider, labels=["Sum"],
               colors=[QColorConstants.DarkCyan], plot_type=PlotType.TimeSeries)
    panel.plot(mean_provider, labels=["Mean"],
               colors=[QColorConstants.DarkYellow], plot_type=PlotType.TimeSeries)

    return panel


# ── Main ────────────────────────────────────────────────────────────────────

app = QApplication(sys.argv)

window = QMainWindow()
window.setWindowTitle("SciQLopPlots DSP Gallery")
window.resize(1200, 800)

tabs = QTabWidget()
tabs.addTab(create_gap_detection_tab(), "Gap Detection")
tabs.addTab(create_nan_interpolation_tab(), "NaN Interpolation")
tabs.addTab(create_resampling_tab(), "Resampling")
tabs.addTab(create_filter_tab(), "Filtering")
tabs.addTab(create_fft_tab(), "FFT")
tabs.addTab(create_spectrogram_tab(), "Spectrogram")
tabs.addTab(create_rolling_stats_tab(), "Rolling Stats")
tabs.addTab(create_reduce_tab(), "Dimension Reduction")

window.setCentralWidget(tabs)
window.show()

sys.exit(app.exec())

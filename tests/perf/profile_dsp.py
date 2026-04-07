"""Run each DSP function in a tight loop for perf profiling.

Usage: perf record -g python tests/perf/profile_dsp.py <function_name> [ncols]
"""
import sys
import numpy as np
from scipy import signal as sp_signal

from SciQLopPlots.dsp import (
    interpolate_nan, resample, fir_filter, iir_sos,
    fft, spectrogram, rolling_mean, rolling_std, reduce,
)

N = 1_000_000
ITERS = 50

ncols = int(sys.argv[2]) if len(sys.argv) > 2 else 1

fs = 1000.0
t = np.arange(N, dtype=np.float64) / fs

if ncols == 1:
    y = np.sin(2 * np.pi * 5 * t) + 0.5 * np.sin(2 * np.pi * 20 * t)
else:
    cols = [np.sin(2 * np.pi * (3 + i) * t) for i in range(ncols)]
    y = np.column_stack(cols)

dt = 1.0 / fs

rng = np.random.default_rng(42)
t_jit = t + rng.uniform(-dt * 0.2, dt * 0.2, size=N)
t_jit = np.sort(t_jit)

coeffs = sp_signal.firwin(65, 10.0, fs=fs)
sos = sp_signal.butter(4, 10.0, fs=fs, output='sos')

funcs = {
    "resample":     lambda: resample(t_jit, y, target_dt=dt, has_gaps=False),
    "fir_filter":   lambda: fir_filter(t, y, coeffs, has_gaps=False),
    "iir_sos":      lambda: iir_sos(t, y, sos, has_gaps=False),
    "fft":          lambda: fft(t, y, has_gaps=False),
    "spectrogram":  lambda: spectrogram(t, y, 0, 256, 128, has_gaps=False),
    "rolling_mean": lambda: rolling_mean(t, y, 51, has_gaps=False),
    "rolling_std":  lambda: rolling_std(t, y, 51, has_gaps=False),
    "reduce":       lambda: reduce(t, y, 'norm', has_gaps=False),
}

name = sys.argv[1] if len(sys.argv) > 1 else "fir_filter"
fn = funcs.get(name)
if not fn:
    print(f"Unknown function: {name}. Choose from: {', '.join(funcs)}")
    sys.exit(1)

print(f"Profiling {name} ({N} samples, {ncols} cols, {ITERS} iterations)...")
for _ in range(ITERS):
    fn()
print("Done.")

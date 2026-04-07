/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/

// CPython extension exposing sqp::dsp functions to Python via numpy arrays.
// No Qt dependency — pure DSP computation.

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <SciQLopPlots/DSP/DSP.hpp>

#include <cstring>
#include <span>
#include <vector>

namespace
{

// ── Numpy helpers ────────────────────────────────────────────────────────────

struct NpArray
{
    PyArrayObject* arr = nullptr;
    double* data = nullptr;
    npy_intp nrows = 0;
    npy_intp ncols = 1;

    ~NpArray() { Py_XDECREF(reinterpret_cast<PyObject*>(arr)); }

    bool parse(PyObject* obj)
    {
        arr = reinterpret_cast<PyArrayObject*>(
            PyArray_FROMANY(obj, NPY_DOUBLE, 1, 2, NPY_ARRAY_C_CONTIGUOUS));
        if (!arr)
            return false;
        data = static_cast<double*>(PyArray_DATA(arr));
        nrows = PyArray_DIM(arr, 0);
        ncols = (PyArray_NDIM(arr) > 1) ? PyArray_DIM(arr, 1) : 1;
        return true;
    }

    std::span<const double> flat_span() const
    {
        return { data, static_cast<std::size_t>(nrows * ncols) };
    }

    std::span<const double> x_span() const { return { data, static_cast<std::size_t>(nrows) }; }
};

PyObject* vec_to_1d(const std::vector<double>& v)
{
    npy_intp dims[1] = { static_cast<npy_intp>(v.size()) };
    PyObject* arr = PyArray_SimpleNew(1, dims, NPY_DOUBLE);
    if (arr)
        std::memcpy(PyArray_DATA(reinterpret_cast<PyArrayObject*>(arr)), v.data(),
            v.size() * sizeof(double));
    return arr;
}

PyObject* vec_to_2d(const std::vector<double>& v, npy_intp rows, npy_intp cols)
{
    npy_intp dims[2] = { rows, cols };
    PyObject* arr = PyArray_SimpleNew(2, dims, NPY_DOUBLE);
    if (arr)
        std::memcpy(PyArray_DATA(reinterpret_cast<PyArrayObject*>(arr)), v.data(),
            v.size() * sizeof(double));
    return arr;
}

PyObject* timeseries_to_tuple(const sqp::dsp::TimeSeries<double>& ts)
{
    PyObject* x = vec_to_1d(ts.x);
    if (!x)
        return nullptr;

    PyObject* y = (ts.n_cols == 1)
        ? vec_to_1d(ts.y)
        : vec_to_2d(
              ts.y, static_cast<npy_intp>(ts.n_rows()), static_cast<npy_intp>(ts.n_cols));

    if (!y)
    {
        Py_DECREF(x);
        return nullptr;
    }

    PyObject* tuple = PyTuple_Pack(2, x, y);
    Py_DECREF(x);
    Py_DECREF(y);
    return tuple;
}

// ── Enum parsers ─────────────────────────────────────────────────────────────

sqp::dsp::WindowType parse_window_type(const char* s)
{
    if (std::strcmp(s, "hamming") == 0)
        return sqp::dsp::WindowType::Hamming;
    if (std::strcmp(s, "blackman") == 0)
        return sqp::dsp::WindowType::Blackman;
    if (std::strcmp(s, "flattop") == 0)
        return sqp::dsp::WindowType::FlatTop;
    if (std::strcmp(s, "rectangular") == 0)
        return sqp::dsp::WindowType::Rectangular;
    return sqp::dsp::WindowType::Hann;
}

sqp::dsp::ReduceOp parse_reduce_op(const char* s)
{
    if (std::strcmp(s, "mean") == 0)
        return sqp::dsp::ReduceOp::Mean;
    if (std::strcmp(s, "min") == 0)
        return sqp::dsp::ReduceOp::Min;
    if (std::strcmp(s, "max") == 0)
        return sqp::dsp::ReduceOp::Max;
    if (std::strcmp(s, "norm") == 0)
        return sqp::dsp::ReduceOp::Norm;
    return sqp::dsp::ReduceOp::Sum;
}

// ── Common pipeline helper ───────────────────────────────────────────────────

// Split → apply stage → reassemble → return (x, y) tuple.
// Releases GIL during computation.
PyObject* apply_stage(
    NpArray& x, NpArray& y, double gap_factor, const sqp::dsp::Stage<double>& stage)
{
    sqp::dsp::TimeSeries<double> ts;
    Py_BEGIN_ALLOW_THREADS auto segments = sqp::dsp::split_segments<double>(
        x.x_span(), y.flat_span(), static_cast<std::size_t>(y.ncols), gap_factor);
    auto results = stage(segments);
    ts = sqp::dsp::detail::reassemble(results);
    Py_END_ALLOW_THREADS return timeseries_to_tuple(ts);
}

// ── Module functions ─────────────────────────────────────────────────────────

PyObject* dsp_split_segments(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    double gap_factor = 3.0;

    static const char* kwlist[] = { "x", "y", "gap_factor", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OO|d", const_cast<char**>(kwlist), &x_obj, &y_obj, &gap_factor))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    std::vector<std::size_t> gap_indices;
    Py_BEGIN_ALLOW_THREADS gap_indices
        = sqp::dsp::detail::find_gap_indices(x.x_span(), gap_factor);
    Py_END_ALLOW_THREADS

        // Build list of (start, end) tuples
        PyObject* result
        = PyList_New(0);
    if (!result)
        return nullptr;

    std::size_t start = 0;
    auto append_segment = [&](std::size_t end)
    {
        PyObject* t = Py_BuildValue("(nn)", static_cast<Py_ssize_t>(start),
            static_cast<Py_ssize_t>(end));
        if (!t || PyList_Append(result, t) < 0)
        {
            Py_XDECREF(t);
            return false;
        }
        Py_DECREF(t);
        start = end;
        return true;
    };

    for (auto idx : gap_indices)
    {
        if (!append_segment(idx))
        {
            Py_DECREF(result);
            return nullptr;
        }
    }
    if (!append_segment(static_cast<std::size_t>(x.nrows)))
    {
        Py_DECREF(result);
        return nullptr;
    }
    return result;
}

PyObject* dsp_interpolate_nan(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t max_consecutive = 1;

    static const char* kwlist[] = { "x", "y", "max_consecutive", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OO|n", const_cast<char**>(kwlist), &x_obj, &y_obj, &max_consecutive))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    std::vector<double> out;
    Py_BEGIN_ALLOW_THREADS out = sqp::dsp::interpolate_nan<double>(x.data, y.data,
        static_cast<std::size_t>(y.nrows), static_cast<std::size_t>(y.ncols),
        static_cast<std::size_t>(max_consecutive));
    Py_END_ALLOW_THREADS

        return (y.ncols == 1) ? vec_to_1d(out)
                              : vec_to_2d(out, y.nrows, y.ncols);
}

PyObject* dsp_resample(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    double gap_factor = 3.0;
    double target_dt = 0.0;

    static const char* kwlist[] = { "x", "y", "gap_factor", "target_dt", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OO|dd", const_cast<char**>(kwlist), &x_obj, &y_obj, &gap_factor,
            &target_dt))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto stage = sqp::dsp::resample_uniform<double>(target_dt);
    return apply_stage(x, y, gap_factor, stage);
}

PyObject* dsp_fir_filter(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    PyObject* coeffs_obj = nullptr;
    double gap_factor = 3.0;

    static const char* kwlist[] = { "x", "y", "coeffs", "gap_factor", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOO|d", const_cast<char**>(kwlist), &x_obj, &y_obj, &coeffs_obj,
            &gap_factor))
        return nullptr;

    NpArray x, y, coeffs;
    if (!x.parse(x_obj) || !y.parse(y_obj) || !coeffs.parse(coeffs_obj))
        return nullptr;

    auto stage = sqp::dsp::fir_filter<double>(
        { coeffs.data, static_cast<std::size_t>(coeffs.nrows) });
    return apply_stage(x, y, gap_factor, stage);
}

PyObject* dsp_iir_sos(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    PyObject* sos_obj = nullptr;
    double gap_factor = 3.0;

    static const char* kwlist[] = { "x", "y", "sos", "gap_factor", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOO|d", const_cast<char**>(kwlist), &x_obj, &y_obj, &sos_obj,
            &gap_factor))
        return nullptr;

    NpArray x, y, sos;
    if (!x.parse(x_obj) || !y.parse(y_obj) || !sos.parse(sos_obj))
        return nullptr;

    if (sos.ncols != 6)
    {
        PyErr_SetString(PyExc_ValueError, "SOS matrix must have 6 columns [b0,b1,b2,a0,a1,a2]");
        return nullptr;
    }

    auto stage = sqp::dsp::iir_sos<double>(
        { sos.data, static_cast<std::size_t>(sos.nrows * 6) },
        static_cast<std::size_t>(sos.nrows));
    return apply_stage(x, y, gap_factor, stage);
}

PyObject* dsp_fft(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    double gap_factor = 3.0;
    const char* window_str = "hann";

    static const char* kwlist[] = { "x", "y", "gap_factor", "window", nullptr };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|ds", const_cast<char**>(kwlist), &x_obj,
            &y_obj, &gap_factor, &window_str))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto win = parse_window_type(window_str);

    std::vector<sqp::dsp::FFTResult<double>> results;
    Py_BEGIN_ALLOW_THREADS auto segments = sqp::dsp::split_segments<double>(
        x.x_span(), y.flat_span(), static_cast<std::size_t>(y.ncols), gap_factor);
    results = sqp::dsp::fft(segments, win);
    Py_END_ALLOW_THREADS

        // Return list of (freqs, magnitude) tuples
        PyObject* list
        = PyList_New(static_cast<Py_ssize_t>(results.size()));
    if (!list)
        return nullptr;

    for (std::size_t i = 0; i < results.size(); ++i)
    {
        auto& r = results[i];
        PyObject* freqs = vec_to_1d(r.frequencies);
        PyObject* mag = (r.n_cols == 1)
            ? vec_to_1d(r.magnitude)
            : vec_to_2d(r.magnitude,
                  static_cast<npy_intp>(r.frequencies.size()), static_cast<npy_intp>(r.n_cols));

        if (!freqs || !mag)
        {
            Py_XDECREF(freqs);
            Py_XDECREF(mag);
            Py_DECREF(list);
            return nullptr;
        }

        PyObject* tuple = PyTuple_Pack(2, freqs, mag);
        Py_DECREF(freqs);
        Py_DECREF(mag);
        if (!tuple)
        {
            Py_DECREF(list);
            return nullptr;
        }
        PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), tuple);
    }
    return list;
}

PyObject* dsp_spectrogram(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t col = 0;
    Py_ssize_t window_size = 256;
    Py_ssize_t overlap = 0;
    double gap_factor = 3.0;
    const char* window_str = "hann";

    static const char* kwlist[]
        = { "x", "y", "col", "window_size", "overlap", "gap_factor", "window", nullptr };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|nnnds", const_cast<char**>(kwlist),
            &x_obj, &y_obj, &col, &window_size, &overlap, &gap_factor, &window_str))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto win = parse_window_type(window_str);

    std::vector<sqp::dsp::SpectrogramResult<double>> results;
    Py_BEGIN_ALLOW_THREADS auto segments = sqp::dsp::split_segments<double>(
        x.x_span(), y.flat_span(), static_cast<std::size_t>(y.ncols), gap_factor);
    results = sqp::dsp::spectrogram(segments, static_cast<std::size_t>(col),
        static_cast<std::size_t>(window_size), static_cast<std::size_t>(overlap), win);
    Py_END_ALLOW_THREADS

        // Return list of (t, f, power) tuples
        PyObject* list
        = PyList_New(static_cast<Py_ssize_t>(results.size()));
    if (!list)
        return nullptr;

    for (std::size_t i = 0; i < results.size(); ++i)
    {
        auto& r = results[i];
        PyObject* t_arr = vec_to_1d(r.t);
        PyObject* f_arr = vec_to_1d(r.f);
        PyObject* p_arr = vec_to_2d(r.power, static_cast<npy_intp>(r.t.size()),
            static_cast<npy_intp>(r.n_freq));

        if (!t_arr || !f_arr || !p_arr)
        {
            Py_XDECREF(t_arr);
            Py_XDECREF(f_arr);
            Py_XDECREF(p_arr);
            Py_DECREF(list);
            return nullptr;
        }

        PyObject* tuple = PyTuple_Pack(3, t_arr, f_arr, p_arr);
        Py_DECREF(t_arr);
        Py_DECREF(f_arr);
        Py_DECREF(p_arr);
        if (!tuple)
        {
            Py_DECREF(list);
            return nullptr;
        }
        PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), tuple);
    }
    return list;
}

PyObject* dsp_rolling_mean(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t window = 51;
    double gap_factor = 3.0;

    static const char* kwlist[] = { "x", "y", "window", "gap_factor", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOn|d", const_cast<char**>(kwlist), &x_obj, &y_obj, &window,
            &gap_factor))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto stage = sqp::dsp::rolling_mean<double>(static_cast<std::size_t>(window));
    return apply_stage(x, y, gap_factor, stage);
}

PyObject* dsp_rolling_std(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t window = 51;
    double gap_factor = 3.0;

    static const char* kwlist[] = { "x", "y", "window", "gap_factor", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOn|d", const_cast<char**>(kwlist), &x_obj, &y_obj, &window,
            &gap_factor))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto stage = sqp::dsp::rolling_std<double>(static_cast<std::size_t>(window));
    return apply_stage(x, y, gap_factor, stage);
}

PyObject* dsp_reduce(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    const char* op_str = "sum";
    double gap_factor = 3.0;

    static const char* kwlist[] = { "x", "y", "op", "gap_factor", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOs|d", const_cast<char**>(kwlist), &x_obj, &y_obj, &op_str,
            &gap_factor))
        return nullptr;

    NpArray x, y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto stage = sqp::dsp::reduce<double>(parse_reduce_op(op_str));
    return apply_stage(x, y, gap_factor, stage);
}

// ── Module definition ────────────────────────────────────────────────────────

// clang-format off
PyMethodDef methods[] = {
    {"split_segments", reinterpret_cast<PyCFunction>(dsp_split_segments),
     METH_VARARGS | METH_KEYWORDS,
     "split_segments(x, y, gap_factor=3.0) -> list[(start, end)]\n"
     "Detect data gaps and return segment index ranges."},

    {"interpolate_nan", reinterpret_cast<PyCFunction>(dsp_interpolate_nan),
     METH_VARARGS | METH_KEYWORDS,
     "interpolate_nan(x, y, max_consecutive=1) -> y_out\n"
     "Linearly interpolate isolated NaN runs (up to max_consecutive)."},

    {"resample", reinterpret_cast<PyCFunction>(dsp_resample),
     METH_VARARGS | METH_KEYWORDS,
     "resample(x, y, gap_factor=3.0, target_dt=0.0) -> (x_out, y_out)\n"
     "Resample to uniform grid per segment. target_dt=0 uses median dt."},

    {"fir_filter", reinterpret_cast<PyCFunction>(dsp_fir_filter),
     METH_VARARGS | METH_KEYWORDS,
     "fir_filter(x, y, coeffs, gap_factor=3.0) -> (x_out, y_out)\n"
     "FIR filter per segment. coeffs: 1D array of filter taps (e.g. from scipy.signal.firwin)."},

    {"iir_sos", reinterpret_cast<PyCFunction>(dsp_iir_sos),
     METH_VARARGS | METH_KEYWORDS,
     "iir_sos(x, y, sos, gap_factor=3.0) -> (x_out, y_out)\n"
     "IIR filter per segment. sos: (n_sections, 6) SOS matrix [b0,b1,b2,a0,a1,a2]\n"
     "(e.g. from scipy.signal.butter(..., output='sos'))."},

    {"fft", reinterpret_cast<PyCFunction>(dsp_fft),
     METH_VARARGS | METH_KEYWORDS,
     "fft(x, y, gap_factor=3.0, window='hann') -> list[(freqs, magnitude)]\n"
     "Per-segment FFT. Returns one (freqs, magnitude) tuple per segment."},

    {"spectrogram", reinterpret_cast<PyCFunction>(dsp_spectrogram),
     METH_VARARGS | METH_KEYWORDS,
     "spectrogram(x, y, col=0, window_size=256, overlap=0, gap_factor=3.0, window='hann')\n"
     "  -> list[(t, f, power)]\n"
     "Per-segment spectrogram. Returns one (t, f, power_2d) tuple per segment."},

    {"rolling_mean", reinterpret_cast<PyCFunction>(dsp_rolling_mean),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_mean(x, y, window, gap_factor=3.0) -> (x_out, y_out)\n"
     "Gap-aware rolling mean."},

    {"rolling_std", reinterpret_cast<PyCFunction>(dsp_rolling_std),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_std(x, y, window, gap_factor=3.0) -> (x_out, y_out)\n"
     "Gap-aware rolling standard deviation."},

    {"reduce", reinterpret_cast<PyCFunction>(dsp_reduce),
     METH_VARARGS | METH_KEYWORDS,
     "reduce(x, y, op, gap_factor=3.0) -> (x_out, y_out)\n"
     "Reduce columns to 1. op: 'sum','mean','min','max','norm'."},

    {nullptr, nullptr, 0, nullptr},
};
// clang-format on

PyModuleDef module_def = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_sciqlop_dsp",
    .m_doc = "C++ DSP functions for SciQLopPlots (gap-aware, SIMD, multi-threaded).",
    .m_size = -1,
    .m_methods = methods,
};

} // anonymous namespace

PyMODINIT_FUNC PyInit__sciqlop_dsp()
{
    import_array();
    return PyModule_Create(&module_def);
}

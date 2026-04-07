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
// Preserves input dtype (float64, float32, int32) through the C++ pipeline.
// No Qt dependency — pure DSP computation.

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <SciQLopPlots/DSP/DSP.hpp>

#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

namespace
{

// ── Numpy type traits ────────────────────────────────────────────────────────

template <typename T>
constexpr int npy_typenum()
{
    if constexpr (std::is_same_v<T, double>)
        return NPY_DOUBLE;
    else if constexpr (std::is_same_v<T, float>)
        return NPY_FLOAT;
    else if constexpr (std::is_same_v<T, int32_t>)
        return NPY_INT32;
}

// ── Dtype-preserving array wrapper ───────────────────────────────────────────

// Timestamps (x): always double.
struct XArray
{
    PyArrayObject* arr = nullptr;
    double* data = nullptr;
    npy_intp nrows = 0;

    ~XArray() { Py_XDECREF(reinterpret_cast<PyObject*>(arr)); }

    bool parse(PyObject* obj)
    {
        arr = reinterpret_cast<PyArrayObject*>(
            PyArray_FROMANY(obj, NPY_DOUBLE, 1, 1, NPY_ARRAY_C_CONTIGUOUS));
        if (!arr)
            return false;
        data = static_cast<double*>(PyArray_DATA(arr));
        nrows = PyArray_DIM(arr, 0);
        return true;
    }

    std::span<const double> span() const
    {
        return { data, static_cast<std::size_t>(nrows) };
    }
};

// Value data (y, coefficients): preserves original dtype.
struct YArray
{
    PyArrayObject* arr = nullptr;
    void* data = nullptr;
    npy_intp nrows = 0;
    npy_intp ncols = 1;
    int dtype = NPY_DOUBLE;

    ~YArray() { Py_XDECREF(reinterpret_cast<PyObject*>(arr)); }

    bool parse(PyObject* obj)
    {
        arr = reinterpret_cast<PyArrayObject*>(
            PyArray_FromAny(obj, nullptr, 1, 2, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED,
                nullptr));
        if (!arr)
            return false;

        dtype = PyArray_TYPE(arr);
        if (dtype != NPY_DOUBLE && dtype != NPY_FLOAT && dtype != NPY_INT32)
        {
            PyErr_SetString(
                PyExc_TypeError, "Array dtype must be float64, float32, or int32");
            Py_DECREF(reinterpret_cast<PyObject*>(arr));
            arr = nullptr;
            return false;
        }

        data = PyArray_DATA(arr);
        nrows = PyArray_DIM(arr, 0);
        ncols = (PyArray_NDIM(arr) > 1) ? PyArray_DIM(arr, 1) : 1;
        return true;
    }


    template <typename T>
    T* typed_data() const
    {
        return static_cast<T*>(data);
    }

    template <typename T>
    std::span<const T> flat_span() const
    {
        return { typed_data<T>(), static_cast<std::size_t>(nrows * ncols) };
    }
};

// ── Output helpers (templated on value type) ─────────────────────────────────

PyObject* double_vec_to_1d(const std::vector<double>& v)
{
    npy_intp dims[1] = { static_cast<npy_intp>(v.size()) };
    PyObject* arr = PyArray_SimpleNew(1, dims, NPY_DOUBLE);
    if (arr)
        std::memcpy(PyArray_DATA(reinterpret_cast<PyArrayObject*>(arr)), v.data(),
            v.size() * sizeof(double));
    return arr;
}

template <typename T>
PyObject* vec_to_1d(const std::vector<T>& v)
{
    npy_intp dims[1] = { static_cast<npy_intp>(v.size()) };
    PyObject* arr = PyArray_SimpleNew(1, dims, npy_typenum<T>());
    if (arr)
        std::memcpy(
            PyArray_DATA(reinterpret_cast<PyArrayObject*>(arr)), v.data(), v.size() * sizeof(T));
    return arr;
}

template <typename T>
PyObject* vec_to_2d(const std::vector<T>& v, npy_intp rows, npy_intp cols)
{
    npy_intp dims[2] = { rows, cols };
    PyObject* arr = PyArray_SimpleNew(2, dims, npy_typenum<T>());
    if (arr)
        std::memcpy(
            PyArray_DATA(reinterpret_cast<PyArrayObject*>(arr)), v.data(), v.size() * sizeof(T));
    return arr;
}

template <typename T>
PyObject* timeseries_to_tuple(const sqp::dsp::TimeSeries<T>& ts)
{
    PyObject* x = double_vec_to_1d(ts.x); // timestamps always double
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

// Preallocate a numpy array and return a (PyObject*, T*) pair.
// Caller owns the PyObject* reference.
template <typename T>
std::pair<PyObject*, T*> alloc_numpy_1d(npy_intp n)
{
    npy_intp dims[1] = { n };
    auto* arr = PyArray_SimpleNew(1, dims, npy_typenum<T>());
    if (!arr)
        return { nullptr, nullptr };
    return { arr, static_cast<T*>(PyArray_DATA(reinterpret_cast<PyArrayObject*>(arr))) };
}

template <typename T>
std::pair<PyObject*, T*> alloc_numpy_2d(npy_intp rows, npy_intp cols)
{
    npy_intp dims[2] = { rows, cols };
    auto* arr = PyArray_SimpleNew(2, dims, npy_typenum<T>());
    if (!arr)
        return { nullptr, nullptr };
    return { arr, static_cast<T*>(PyArray_DATA(reinterpret_cast<PyArrayObject*>(arr))) };
}

// Return (x_ref, y_arr) tuple where x_ref is the input x array (borrowed+incref'd)
// and y_arr is a newly allocated numpy array. Returns the y data pointer for C++ to write into.
template <typename T>
struct ZeroCopyOutput
{
    PyObject* x_ref = nullptr;  // borrowed ref to input x, incref'd
    PyObject* y_obj = nullptr;  // new numpy array
    T* y_ptr = nullptr;         // raw pointer into y_obj

    bool alloc_like(XArray& x, YArray& y)
    {
        x_ref = reinterpret_cast<PyObject*>(x.arr);
        Py_INCREF(x_ref);
        if (y.ncols == 1)
        {
            auto [obj, ptr] = alloc_numpy_1d<T>(y.nrows);
            y_obj = obj;
            y_ptr = ptr;
        }
        else
        {
            auto [obj, ptr] = alloc_numpy_2d<T>(y.nrows, y.ncols);
            y_obj = obj;
            y_ptr = ptr;
        }
        return y_obj != nullptr;
    }

    bool alloc_reduced(XArray& x, npy_intp nrows)
    {
        x_ref = reinterpret_cast<PyObject*>(x.arr);
        Py_INCREF(x_ref);
        auto [obj, ptr] = alloc_numpy_1d<T>(nrows);
        y_obj = obj;
        y_ptr = ptr;
        return y_obj != nullptr;
    }

    PyObject* to_tuple()
    {
        auto* t = PyTuple_Pack(2, x_ref, y_obj);
        Py_DECREF(x_ref);
        Py_DECREF(y_obj);
        x_ref = nullptr;
        y_obj = nullptr;
        return t;
    }

    void cleanup()
    {
        Py_XDECREF(x_ref);
        Py_XDECREF(y_obj);
    }
};

// ── Dtype dispatch ───────────────────────────────────────────────────────────

// Dispatch a generic lambda on the y array's dtype.
// The lambda must be a template: [&]<typename T>() -> PyObject*
template <typename F>
PyObject* dispatch(int dtype, F&& f)
{
    switch (dtype)
    {
        case NPY_DOUBLE:
            return f.template operator()<double>();
        case NPY_FLOAT:
            return f.template operator()<float>();
        case NPY_INT32:
            return f.template operator()<int32_t>();
        default:
            PyErr_SetString(PyExc_TypeError, "Unsupported dtype (need float64, float32, or int32)");
            return nullptr;
    }
}

// ── Common pipeline helper ───────────────────────────────────────────────────

template <typename T>
PyObject* apply_stage(
    XArray& x, YArray& y, double gap_factor, bool has_gaps, const sqp::dsp::Stage<T>& stage)
{
    sqp::dsp::TimeSeries<T> ts;
    Py_BEGIN_ALLOW_THREADS
    if (has_gaps)
    {
        auto segments = sqp::dsp::split_segments<T>(
            x.span(), y.flat_span<T>(), static_cast<std::size_t>(y.ncols), gap_factor);
        auto results = stage(segments);
        ts = sqp::dsp::detail::reassemble(results);
    }
    else
    {
        std::vector<sqp::dsp::Segment<T>> segments { sqp::dsp::Segment<T> {
            .x = x.span(),
            .y = y.flat_span<T>(),
            .n_cols = static_cast<std::size_t>(y.ncols),
            .median_dt = sqp::dsp::detail::compute_median_dt(x.span()),
        } };
        auto results = stage(segments);
        ts = std::move(results.front());
    }
    Py_END_ALLOW_THREADS
    return timeseries_to_tuple(ts);
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

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    std::vector<std::size_t> gap_indices;
    Py_BEGIN_ALLOW_THREADS
    gap_indices = sqp::dsp::detail::find_gap_indices(x.span(), gap_factor);
    Py_END_ALLOW_THREADS

    PyObject* result = PyList_New(0);
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

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<T> out;
        Py_BEGIN_ALLOW_THREADS
        out = sqp::dsp::interpolate_nan<T>(x.data, y.typed_data<T>(),
            static_cast<std::size_t>(y.nrows), static_cast<std::size_t>(y.ncols),
            static_cast<std::size_t>(max_consecutive));
        Py_END_ALLOW_THREADS
        return (y.ncols == 1) ? vec_to_1d(out) : vec_to_2d(out, y.nrows, y.ncols);
    });
}

PyObject* dsp_resample(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    double gap_factor = 3.0;
    double target_dt = 0.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "gap_factor", "target_dt", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OO|ddp", const_cast<char**>(kwlist), &x_obj, &y_obj, &gap_factor,
            &target_dt, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps && x.nrows >= 2)
        {
            const double dt = (target_dt > 0.0)
                ? target_dt
                : sqp::dsp::detail::compute_median_dt(x.span());
            if (dt <= 0.0)
            {
                // Degenerate: return input as-is (incref both)
                Py_INCREF(reinterpret_cast<PyObject*>(x.arr));
                Py_INCREF(reinterpret_cast<PyObject*>(y.arr));
                return PyTuple_Pack(2,
                    reinterpret_cast<PyObject*>(x.arr),
                    reinterpret_cast<PyObject*>(y.arr));
            }

            const double x_start = x.data[0];
            const double x_end = x.data[x.nrows - 1];
            const auto n_out = static_cast<npy_intp>(
                std::floor((x_end - x_start) / dt)) + 1;
            const auto ncols = static_cast<std::size_t>(y.ncols);

            // Preallocate output numpy arrays
            auto [x_out_obj, x_out_ptr] = alloc_numpy_1d<double>(n_out);
            if (!x_out_obj)
                return static_cast<PyObject*>(nullptr);

            PyObject* y_out_obj;
            T* y_out_ptr;
            if (ncols == 1)
            {
                auto [obj, ptr] = alloc_numpy_1d<T>(n_out);
                y_out_obj = obj;
                y_out_ptr = ptr;
            }
            else
            {
                auto [obj, ptr] = alloc_numpy_2d<T>(n_out, static_cast<npy_intp>(ncols));
                y_out_obj = obj;
                y_out_ptr = ptr;
            }
            if (!y_out_obj)
            {
                Py_DECREF(x_out_obj);
                return static_cast<PyObject*>(nullptr);
            }

            Py_BEGIN_ALLOW_THREADS
            for (npy_intp i = 0; i < n_out; ++i)
                x_out_ptr[i] = x_start + static_cast<double>(i) * dt;

            for (std::size_t col = 0; col < ncols; ++col)
            {
                sqp::dsp::detail::interp_column(
                    x.data, y.typed_data<T>(),
                    static_cast<std::size_t>(x.nrows), ncols, col,
                    x_out_ptr, y_out_ptr,
                    static_cast<std::size_t>(n_out), ncols);
            }
            Py_END_ALLOW_THREADS

            auto* tuple = PyTuple_Pack(2, x_out_obj, y_out_obj);
            Py_DECREF(x_out_obj);
            Py_DECREF(y_out_obj);
            return tuple;
        }
        auto stage = sqp::dsp::resample_uniform<T>(target_dt);
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_fir_filter(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    PyObject* coeffs_obj = nullptr;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "coeffs", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOO|dp", const_cast<char**>(kwlist), &x_obj, &y_obj, &coeffs_obj,
            &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y, coeffs;
    if (!x.parse(x_obj) || !y.parse(y_obj) || !coeffs.parse(coeffs_obj))
        return nullptr;

    if (y.dtype != coeffs.dtype)
    {
        PyErr_SetString(PyExc_TypeError, "y and coeffs must have the same dtype");
        return nullptr;
    }

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_like(x, y))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            Py_BEGIN_ALLOW_THREADS
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::fir_apply_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    coeffs.typed_data<T>(), static_cast<std::size_t>(coeffs.nrows), out.y_ptr);
            Py_END_ALLOW_THREADS
            return out.to_tuple();
        }
        auto stage = sqp::dsp::fir_filter<T>(
            { coeffs.typed_data<T>(), static_cast<std::size_t>(coeffs.nrows) });
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_iir_sos(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    PyObject* sos_obj = nullptr;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "sos", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOO|dp", const_cast<char**>(kwlist), &x_obj, &y_obj, &sos_obj,
            &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y, sos;
    if (!x.parse(x_obj) || !y.parse(y_obj) || !sos.parse(sos_obj))
        return nullptr;

    if (y.dtype != sos.dtype)
    {
        PyErr_SetString(PyExc_TypeError, "y and sos must have the same dtype");
        return nullptr;
    }

    if (sos.ncols != 6)
    {
        PyErr_SetString(PyExc_ValueError, "SOS matrix must have 6 columns [b0,b1,b2,a0,a1,a2]");
        return nullptr;
    }

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_like(x, y))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            Py_BEGIN_ALLOW_THREADS
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::sos_apply_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    sos.typed_data<T>(), static_cast<std::size_t>(sos.nrows), out.y_ptr);
            Py_END_ALLOW_THREADS
            return out.to_tuple();
        }
        auto stage = sqp::dsp::iir_sos<T>(
            { sos.typed_data<T>(), static_cast<std::size_t>(sos.nrows * 6) },
            static_cast<std::size_t>(sos.nrows));
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_filtfilt(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    PyObject* coeffs_obj = nullptr;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "coeffs", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOO|dp", const_cast<char**>(kwlist), &x_obj, &y_obj, &coeffs_obj,
            &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y, coeffs;
    if (!x.parse(x_obj) || !y.parse(y_obj) || !coeffs.parse(coeffs_obj))
        return nullptr;

    if (y.dtype != coeffs.dtype)
    {
        PyErr_SetString(PyExc_TypeError, "y and coeffs must have the same dtype");
        return nullptr;
    }

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_like(x, y))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            Py_BEGIN_ALLOW_THREADS
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::filtfilt_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    coeffs.typed_data<T>(), static_cast<std::size_t>(coeffs.nrows), out.y_ptr);
            Py_END_ALLOW_THREADS
            return out.to_tuple();
        }
        // Gap-aware: build segments, apply filtfilt per segment
        auto coeffs_vec = std::vector<T>(
            coeffs.typed_data<T>(), coeffs.typed_data<T>() + coeffs.nrows);
        auto stage = [coeffs_vec](const std::vector<sqp::dsp::Segment<T>>& segments)
            -> std::vector<sqp::dsp::TimeSeries<T>>
        {
            std::vector<sqp::dsp::TimeSeries<T>> results(segments.size());
            sqp::dsp::parallel_for(segments.size(), [&](std::size_t i) {
                const auto& seg = segments[i];
                auto& r = results[i];
                r.x.assign(seg.x.begin(), seg.x.end());
                r.y.resize(seg.y.size());
                r.n_cols = seg.n_cols;
                for (std::size_t col = 0; col < seg.n_cols; ++col)
                    sqp::dsp::detail::filtfilt_column(
                        seg.y.data(), seg.x.size(), seg.n_cols, col,
                        coeffs_vec.data(), coeffs_vec.size(), r.y.data());
            });
            return results;
        };
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_sosfiltfilt(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    PyObject* sos_obj = nullptr;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "sos", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOO|dp", const_cast<char**>(kwlist), &x_obj, &y_obj, &sos_obj,
            &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y, sos;
    if (!x.parse(x_obj) || !y.parse(y_obj) || !sos.parse(sos_obj))
        return nullptr;

    if (y.dtype != sos.dtype)
    {
        PyErr_SetString(PyExc_TypeError, "y and sos must have the same dtype");
        return nullptr;
    }

    if (sos.ncols != 6)
    {
        PyErr_SetString(PyExc_ValueError, "SOS matrix must have 6 columns [b0,b1,b2,a0,a1,a2]");
        return nullptr;
    }

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_like(x, y))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            Py_BEGIN_ALLOW_THREADS
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::sosfiltfilt_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    sos.typed_data<T>(), static_cast<std::size_t>(sos.nrows), out.y_ptr);
            Py_END_ALLOW_THREADS
            return out.to_tuple();
        }
        auto sos_vec = std::vector<T>(
            sos.typed_data<T>(), sos.typed_data<T>() + sos.nrows * 6);
        auto n_sec = static_cast<std::size_t>(sos.nrows);
        auto stage = [sos_vec, n_sec](const std::vector<sqp::dsp::Segment<T>>& segments)
            -> std::vector<sqp::dsp::TimeSeries<T>>
        {
            std::vector<sqp::dsp::TimeSeries<T>> results(segments.size());
            sqp::dsp::parallel_for(segments.size(), [&](std::size_t i) {
                const auto& seg = segments[i];
                auto& r = results[i];
                r.x.assign(seg.x.begin(), seg.x.end());
                r.y.resize(seg.y.size());
                r.n_cols = seg.n_cols;
                for (std::size_t col = 0; col < seg.n_cols; ++col)
                    sqp::dsp::detail::sosfiltfilt_column(
                        seg.y.data(), seg.x.size(), seg.n_cols, col,
                        sos_vec.data(), n_sec, r.y.data());
            });
            return results;
        };
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_fft(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    double gap_factor = 3.0;
    const char* window_str = "hann";
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "gap_factor", "window", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|dsp", const_cast<char**>(kwlist), &x_obj,
            &y_obj, &gap_factor, &window_str, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto win = parse_window_type(window_str);

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<sqp::dsp::FFTResult<T>> results;
        Py_BEGIN_ALLOW_THREADS
        std::vector<sqp::dsp::Segment<T>> segments;
        if (has_gaps)
        {
            segments = sqp::dsp::split_segments<T>(
                x.span(), y.flat_span<T>(), static_cast<std::size_t>(y.ncols), gap_factor);
        }
        else
        {
            // No gaps: skip expensive median_dt, use first dt as sample period
            const double dt = (x.nrows >= 2) ? (x.data[1] - x.data[0]) : 1.0;
            segments.push_back(sqp::dsp::Segment<T> {
                .x = x.span(),
                .y = y.flat_span<T>(),
                .n_cols = static_cast<std::size_t>(y.ncols),
                .median_dt = dt,
            });
        }
        results = sqp::dsp::fft(segments, win);
        Py_END_ALLOW_THREADS

        PyObject* list = PyList_New(static_cast<Py_ssize_t>(results.size()));
        if (!list)
            return static_cast<PyObject*>(nullptr);

        for (std::size_t i = 0; i < results.size(); ++i)
        {
            auto& r = results[i];
            PyObject* freqs = double_vec_to_1d(r.frequencies); // frequencies always double
            PyObject* mag = (r.n_cols == 1)
                ? vec_to_1d(r.magnitude)
                : vec_to_2d(r.magnitude,
                      static_cast<npy_intp>(r.frequencies.size()),
                      static_cast<npy_intp>(r.n_cols));

            if (!freqs || !mag)
            {
                Py_XDECREF(freqs);
                Py_XDECREF(mag);
                Py_DECREF(list);
                return static_cast<PyObject*>(nullptr);
            }

            PyObject* tuple = PyTuple_Pack(2, freqs, mag);
            Py_DECREF(freqs);
            Py_DECREF(mag);
            if (!tuple)
            {
                Py_DECREF(list);
                return static_cast<PyObject*>(nullptr);
            }
            PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), tuple);
        }
        return list;
    });
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

    int has_gaps = 1;

    static const char* kwlist[]
        = { "x", "y", "col", "window_size", "overlap", "gap_factor", "window", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|nnndsp", const_cast<char**>(kwlist),
            &x_obj, &y_obj, &col, &window_size, &overlap, &gap_factor, &window_str, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    auto win = parse_window_type(window_str);

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<sqp::dsp::SpectrogramResult<T>> results;
        Py_BEGIN_ALLOW_THREADS
        std::vector<sqp::dsp::Segment<T>> segments;
        if (has_gaps)
        {
            segments = sqp::dsp::split_segments<T>(
                x.span(), y.flat_span<T>(), static_cast<std::size_t>(y.ncols), gap_factor);
        }
        else
        {
            const double dt = (x.nrows >= 2) ? (x.data[1] - x.data[0]) : 1.0;
            segments.push_back(sqp::dsp::Segment<T> {
                .x = x.span(),
                .y = y.flat_span<T>(),
                .n_cols = static_cast<std::size_t>(y.ncols),
                .median_dt = dt,
            });
        }
        results = sqp::dsp::spectrogram(segments, static_cast<std::size_t>(col),
            static_cast<std::size_t>(window_size), static_cast<std::size_t>(overlap), win);
        Py_END_ALLOW_THREADS

        PyObject* list = PyList_New(static_cast<Py_ssize_t>(results.size()));
        if (!list)
            return static_cast<PyObject*>(nullptr);

        for (std::size_t i = 0; i < results.size(); ++i)
        {
            auto& r = results[i];
            PyObject* t_arr = double_vec_to_1d(r.t); // times always double
            PyObject* f_arr = double_vec_to_1d(r.f); // frequencies always double
            PyObject* p_arr = vec_to_2d(r.power, static_cast<npy_intp>(r.t.size()),
                static_cast<npy_intp>(r.n_freq));

            if (!t_arr || !f_arr || !p_arr)
            {
                Py_XDECREF(t_arr);
                Py_XDECREF(f_arr);
                Py_XDECREF(p_arr);
                Py_DECREF(list);
                return static_cast<PyObject*>(nullptr);
            }

            PyObject* tuple = PyTuple_Pack(3, t_arr, f_arr, p_arr);
            Py_DECREF(t_arr);
            Py_DECREF(f_arr);
            Py_DECREF(p_arr);
            if (!tuple)
            {
                Py_DECREF(list);
                return static_cast<PyObject*>(nullptr);
            }
            PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), tuple);
        }
        return list;
    });
}

PyObject* dsp_rolling_mean(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t window = 51;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "window", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOn|dp", const_cast<char**>(kwlist), &x_obj, &y_obj, &window,
            &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_like(x, y))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            const auto win = static_cast<std::size_t>(window);
            Py_BEGIN_ALLOW_THREADS
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::rolling_mean_column(
                    y.typed_data<T>(), nrows, ncols, col, win, out.y_ptr);
            Py_END_ALLOW_THREADS
            return out.to_tuple();
        }
        auto stage = sqp::dsp::rolling_mean<T>(static_cast<std::size_t>(window));
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_rolling_std(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t window = 51;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "window", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOn|dp", const_cast<char**>(kwlist), &x_obj, &y_obj, &window,
            &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_like(x, y))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            const auto win = static_cast<std::size_t>(window);
            Py_BEGIN_ALLOW_THREADS
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::rolling_std_column(
                    y.typed_data<T>(), nrows, ncols, col, win, out.y_ptr);
            Py_END_ALLOW_THREADS
            return out.to_tuple();
        }
        auto stage = sqp::dsp::rolling_std<T>(static_cast<std::size_t>(window));
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_reduce(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    const char* op_str = "sum";
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "op", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOs|dp", const_cast<char**>(kwlist), &x_obj, &y_obj, &op_str,
            &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_reduced(x, y.nrows))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            const auto op = parse_reduce_op(op_str);
            Py_BEGIN_ALLOW_THREADS
            for (std::size_t row = 0; row < nrows; ++row)
                out.y_ptr[row] = sqp::dsp::detail::reduce_row(
                    &y.typed_data<T>()[row * ncols], ncols, op);
            Py_END_ALLOW_THREADS
            return out.to_tuple();
        }
        auto stage = sqp::dsp::reduce<T>(parse_reduce_op(op_str));
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_reduce_axes(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    PyObject* shape_obj = nullptr;
    PyObject* axes_obj = nullptr;
    const char* op_str = "sum";
    int has_gaps = 1;

    static const char* kwlist[] = { "x", "y", "shape", "axes", "op", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "OOOO|sp", const_cast<char**>(kwlist), &x_obj, &y_obj, &shape_obj,
            &axes_obj, &op_str, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;

    // Parse shape tuple
    PyObject* shape_seq = PySequence_Fast(shape_obj, "shape must be a sequence");
    if (!shape_seq)
        return nullptr;
    const auto ndim = static_cast<std::size_t>(PySequence_Fast_GET_SIZE(shape_seq));
    std::vector<std::size_t> shape(ndim);
    std::size_t prod = 1;
    for (std::size_t i = 0; i < ndim; ++i)
    {
        shape[i] = static_cast<std::size_t>(PyLong_AsLong(PySequence_Fast_GET_ITEM(shape_seq, i)));
        prod *= shape[i];
    }
    Py_DECREF(shape_seq);

    if (static_cast<npy_intp>(prod) != y.ncols)
    {
        PyErr_Format(PyExc_ValueError,
            "prod(shape) = %zu does not match n_cols = %zd", prod, y.ncols);
        return nullptr;
    }

    // Parse axes tuple
    PyObject* axes_seq = PySequence_Fast(axes_obj, "axes must be a sequence");
    if (!axes_seq)
        return nullptr;
    const auto n_axes = static_cast<std::size_t>(PySequence_Fast_GET_SIZE(axes_seq));
    std::vector<std::size_t> axes(n_axes);
    for (std::size_t i = 0; i < n_axes; ++i)
        axes[i] = static_cast<std::size_t>(PyLong_AsLong(PySequence_Fast_GET_ITEM(axes_seq, i)));
    Py_DECREF(axes_seq);

    const auto op = parse_reduce_op(op_str);

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        sqp::dsp::detail::ReduceAxesSpec spec(shape.data(), ndim, axes.data(), n_axes);
        const auto nrows = static_cast<std::size_t>(y.nrows);
        const auto out_cols = static_cast<npy_intp>(spec.out_size);

        PyObject* x_ref = reinterpret_cast<PyObject*>(x.arr);
        Py_INCREF(x_ref);

        PyObject* y_out_obj;
        T* y_out_ptr;
        if (out_cols == 1)
        {
            auto [obj, ptr] = alloc_numpy_1d<T>(y.nrows);
            y_out_obj = obj;
            y_out_ptr = ptr;
        }
        else
        {
            auto [obj, ptr] = alloc_numpy_2d<T>(y.nrows, out_cols);
            y_out_obj = obj;
            y_out_ptr = ptr;
        }
        if (!y_out_obj)
        {
            Py_DECREF(x_ref);
            return static_cast<PyObject*>(nullptr);
        }

        Py_BEGIN_ALLOW_THREADS
        sqp::dsp::parallel_for(nrows, [&](std::size_t row) {
            sqp::dsp::detail::reduce_axes_row(
                &y.typed_data<T>()[row * prod], spec, op, &y_out_ptr[row * spec.out_size]);
        });
        Py_END_ALLOW_THREADS

        auto* tuple = PyTuple_Pack(2, x_ref, y_out_obj);
        Py_DECREF(x_ref);
        Py_DECREF(y_out_obj);
        return tuple;
    });
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
     "Linearly interpolate isolated NaN runs (up to max_consecutive).\n"
     "Preserves input dtype (float64/float32; no-op for int32)."},

    {"resample", reinterpret_cast<PyCFunction>(dsp_resample),
     METH_VARARGS | METH_KEYWORDS,
     "resample(x, y, gap_factor=3.0, target_dt=0.0) -> (x_out, y_out)\n"
     "Resample to uniform grid per segment. Preserves y dtype."},

    {"fir_filter", reinterpret_cast<PyCFunction>(dsp_fir_filter),
     METH_VARARGS | METH_KEYWORDS,
     "fir_filter(x, y, coeffs, gap_factor=3.0) -> (x_out, y_out)\n"
     "FIR filter per segment. coeffs auto-converted to match y dtype."},

    {"iir_sos", reinterpret_cast<PyCFunction>(dsp_iir_sos),
     METH_VARARGS | METH_KEYWORDS,
     "iir_sos(x, y, sos, gap_factor=3.0) -> (x_out, y_out)\n"
     "IIR filter per segment. sos: (n_sections, 6) SOS matrix.\n"
     "SOS auto-converted to match y dtype."},

    {"filtfilt", reinterpret_cast<PyCFunction>(dsp_filtfilt),
     METH_VARARGS | METH_KEYWORDS,
     "filtfilt(x, y, coeffs, gap_factor=3.0, has_gaps=True) -> (x_out, y_out)\n"
     "Zero-phase FIR filter (forward-backward). Equivalent to scipy.signal.filtfilt."},

    {"sosfiltfilt", reinterpret_cast<PyCFunction>(dsp_sosfiltfilt),
     METH_VARARGS | METH_KEYWORDS,
     "sosfiltfilt(x, y, sos, gap_factor=3.0, has_gaps=True) -> (x_out, y_out)\n"
     "Zero-phase IIR filter (forward-backward SOS). Equivalent to scipy.signal.sosfiltfilt."},

    {"fft", reinterpret_cast<PyCFunction>(dsp_fft),
     METH_VARARGS | METH_KEYWORDS,
     "fft(x, y, gap_factor=3.0, window='hann') -> list[(freqs, magnitude)]\n"
     "Per-segment FFT. Magnitude preserves y dtype; freqs always float64."},

    {"spectrogram", reinterpret_cast<PyCFunction>(dsp_spectrogram),
     METH_VARARGS | METH_KEYWORDS,
     "spectrogram(x, y, col=0, window_size=256, overlap=0, gap_factor=3.0, window='hann')\n"
     "  -> list[(t, f, power)]\n"
     "Per-segment spectrogram. Power preserves y dtype; t/f always float64."},

    {"rolling_mean", reinterpret_cast<PyCFunction>(dsp_rolling_mean),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_mean(x, y, window, gap_factor=3.0) -> (x_out, y_out)\n"
     "Gap-aware rolling mean. Preserves y dtype."},

    {"rolling_std", reinterpret_cast<PyCFunction>(dsp_rolling_std),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_std(x, y, window, gap_factor=3.0) -> (x_out, y_out)\n"
     "Gap-aware rolling standard deviation. Preserves y dtype."},

    {"reduce", reinterpret_cast<PyCFunction>(dsp_reduce),
     METH_VARARGS | METH_KEYWORDS,
     "reduce(x, y, op, gap_factor=3.0) -> (x_out, y_out)\n"
     "Reduce columns to 1. Preserves y dtype."},

    {"reduce_axes", reinterpret_cast<PyCFunction>(dsp_reduce_axes),
     METH_VARARGS | METH_KEYWORDS,
     "reduce_axes(x, y, shape, axes, op='sum', has_gaps=False) -> (x_out, y_out)\n"
     "Reduce arbitrary axes within each row.\n"
     "shape: tuple decomposing n_cols (e.g. (32, 16, 8) for energy x theta x phi).\n"
     "axes: tuple of axes to reduce (e.g. (1, 2) to sum over angles).\n"
     "Output has prod(kept_shape) columns."},

    {nullptr, nullptr, 0, nullptr},
};
// clang-format on

PyModuleDef module_def = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_sciqlop_dsp",
    .m_doc = "C++ DSP functions for SciQLopPlots.\n"
             "Dtype-preserving (float64/float32/int32), gap-aware, SIMD, multi-threaded.",
    .m_size = -1,
    .m_methods = methods,
};

} // anonymous namespace

PyMODINIT_FUNC PyInit__sciqlop_dsp()
{
    import_array();
    return PyModule_Create(&module_def);
}

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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace
{

// ── Exception-safe GIL release ───────────────────────────────────────────────
//
// `SQDSP_GIL_RELEASE_BEGIN` / `SQDSP_GIL_RELEASE_END` are scope-bracketing
// macros that drop and restore the GIL via plain assignment, not RAII. Any
// C++ exception thrown inside the scope unwinds past the END macro without
// reacquiring the GIL, so the catch handler runs without the GIL and any
// PyErr_* call is undefined behavior — `std::terminate` in practice.
//
// `GILReleaseScope` releases the GIL on construction and reacquires it on
// destruction (including stack unwinding from an exception). Callers wrap
// the body in try/catch; the catch handler always runs with the GIL held.
struct GILReleaseScope
{
    PyThreadState* save;
    GILReleaseScope() : save(PyEval_SaveThread()) {}
    ~GILReleaseScope() { PyEval_RestoreThread(save); }
    GILReleaseScope(const GILReleaseScope&) = delete;
    GILReleaseScope& operator=(const GILReleaseScope&) = delete;
};

#define SQDSP_GIL_RELEASE_BEGIN try { GILReleaseScope _sqdsp_gil_;
#define SQDSP_GIL_RELEASE_END                                                  \
    }                                                                          \
    catch (const std::exception& _sqdsp_e)                                     \
    {                                                                          \
        PyErr_SetString(PyExc_RuntimeError, _sqdsp_e.what());                  \
        return nullptr;                                                        \
    }

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
        if (!is_kernel_dtype(dtype) && is_real_number(arr) && !cast_to_double())
            return false;
        if (!is_kernel_dtype(dtype))
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

    static bool is_kernel_dtype(int t)
    {
        return t == NPY_DOUBLE || t == NPY_FLOAT || t == NPY_INT32;
    }

    static bool is_real_number(PyArrayObject* a)
    {
        return PyArray_ISINTEGER(a) || PyArray_ISBOOL(a) || PyArray_ISFLOAT(a);
    }

    // Kernels exist for three types only; any other real type (int64 above all,
    // NumPy's default integer) is computed in float64, like NumPy's own reductions.
    bool cast_to_double()
    {
        auto* converted = reinterpret_cast<PyArrayObject*>(
            PyArray_FROMANY(reinterpret_cast<PyObject*>(arr), NPY_DOUBLE, 1, 2,
                NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_ALIGNED | NPY_ARRAY_FORCECAST));
        Py_DECREF(reinterpret_cast<PyObject*>(arr));
        arr = converted;
        dtype = NPY_DOUBLE;
        return arr != nullptr;
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

// ── Validation helpers ───────────────────────────────────────────────────

// Reads any iterable of ints; nullopt with a Python error set on failure.
std::optional<std::vector<long>> as_long_vector(PyObject* obj, const char* what)
{
    PyObject* list = PySequence_List(obj);
    if (!list)
    {
        if (PyErr_ExceptionMatches(PyExc_TypeError))
            PyErr_Format(PyExc_TypeError, "%s must be a sequence", what);
        return std::nullopt;
    }
    std::vector<long> values(static_cast<std::size_t>(PyList_Size(list)));
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        values[i] = PyLong_AsLong(PyList_GetItem(list, static_cast<Py_ssize_t>(i)));
        if (values[i] == -1 && PyErr_Occurred())
        {
            Py_DECREF(list);
            return std::nullopt;
        }
    }
    Py_DECREF(list);
    return values;
}

bool check_xy_sizes(XArray& x, YArray& y)
{
    if (x.nrows != y.nrows)
    {
        PyErr_Format(PyExc_ValueError,
            "x length (%zd) must match y row count (%zd)", x.nrows, y.nrows);
        return false;
    }
    return true;
}

bool check_min_rows(XArray& x, Py_ssize_t min_rows, const char* func_name)
{
    if (x.nrows < min_rows)
    {
        PyErr_Format(PyExc_ValueError,
            "%s requires at least %zd samples, got %zd", func_name, min_rows, x.nrows);
        return false;
    }
    return true;
}

bool check_gap_factor(double gap_factor)
{
    if (gap_factor <= 0.0)
    {
        PyErr_SetString(PyExc_ValueError, "gap_factor must be > 0");
        return false;
    }
    return true;
}

bool check_window_size(Py_ssize_t window, Py_ssize_t nrows, const char* func_name)
{
    if (window <= 0)
    {
        PyErr_Format(PyExc_ValueError,
            "%s: window must be > 0, got %zd", func_name, window);
        return false;
    }
    if (window > nrows)
    {
        PyErr_Format(PyExc_ValueError,
            "%s: window (%zd) must be <= number of rows (%zd)", func_name, window, nrows);
        return false;
    }
    return true;
}

bool check_percentile(double q, const char* func_name)
{
    if (!(q >= 0.0 && q <= 100.0))
    {
        // PyErr_Format (PyUnicode_FromFormat) has no %f/%g conversion for
        // doubles, so the value is pre-formatted into a string first.
        char q_str[32];
        std::snprintf(q_str, sizeof(q_str), "%g", q);
        PyErr_Format(PyExc_ValueError,
            "%s: q must be in [0, 100], got %s", func_name, q_str);
        return false;
    }
    return true;
}

bool check_sos_a0(YArray& sos)
{
    // Check that no a0 coefficient is zero (would cause division by zero)
    // sos is (n_sections, 6), a0 is at column index 3
    for (Py_ssize_t s = 0; s < sos.nrows; ++s)
    {
        double a0 = 0.0;
        switch (sos.dtype)
        {
            case NPY_DOUBLE:
                a0 = static_cast<double>(sos.typed_data<double>()[s * 6 + 3]);
                break;
            case NPY_FLOAT:
                a0 = static_cast<double>(sos.typed_data<float>()[s * 6 + 3]);
                break;
            case NPY_INT32:
                a0 = static_cast<double>(sos.typed_data<int32_t>()[s * 6 + 3]);
                break;
        }
        if (a0 == 0.0)
        {
            PyErr_Format(PyExc_ValueError,
                "SOS section %zd has a0=0 (division by zero)", s);
            return false;
        }
    }
    return true;
}

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
    SQDSP_GIL_RELEASE_BEGIN
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
    SQDSP_GIL_RELEASE_END
    return timeseries_to_tuple(ts);
}

// ── Enum parsers ─────────────────────────────────────────────────────────────

template <typename Enum, std::size_t N>
using NameTable = std::array<std::pair<const char*, Enum>, N>;

constexpr NameTable<sqp::dsp::WindowType, 5> window_names { {
    { "hann", sqp::dsp::WindowType::Hann },
    { "hamming", sqp::dsp::WindowType::Hamming },
    { "blackman", sqp::dsp::WindowType::Blackman },
    { "flattop", sqp::dsp::WindowType::FlatTop },
    { "rectangular", sqp::dsp::WindowType::Rectangular },
} };

constexpr NameTable<sqp::dsp::ReduceOp, 5> reduce_op_names { {
    { "sum", sqp::dsp::ReduceOp::Sum },
    { "mean", sqp::dsp::ReduceOp::Mean },
    { "min", sqp::dsp::ReduceOp::Min },
    { "max", sqp::dsp::ReduceOp::Max },
    { "norm", sqp::dsp::ReduceOp::Norm },
} };

// nullopt with a ValueError set, listing the valid names, when `name` is not one of them.
template <typename Enum, std::size_t N>
std::optional<Enum> parse_name(const char* name, const NameTable<Enum, N>& table, const char* what)
{
    for (const auto& [candidate, value] : table)
        if (std::strcmp(name, candidate) == 0)
            return value;
    std::string valid;
    for (const auto& [candidate, value] : table)
        valid += (valid.empty() ? "" : ", ") + std::string(candidate);
    PyErr_Format(PyExc_ValueError, "unknown %s '%s', expected one of: %s", what, name, valid.c_str());
    return std::nullopt;
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;

    std::vector<std::size_t> gap_indices;
    SQDSP_GIL_RELEASE_BEGIN
    gap_indices = sqp::dsp::detail::find_gap_indices(x.span(), gap_factor);
    SQDSP_GIL_RELEASE_END

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
    if (!check_xy_sizes(x, y))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<T> out;
        SQDSP_GIL_RELEASE_BEGIN
        out = sqp::dsp::interpolate_nan<T>(x.data, y.typed_data<T>(),
            static_cast<std::size_t>(y.nrows), static_cast<std::size_t>(y.ncols),
            static_cast<std::size_t>(max_consecutive));
        SQDSP_GIL_RELEASE_END
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;

    if (target_dt > 0.0 && x.nrows >= 2)
    {
        const double span = x.data[x.nrows - 1] - x.data[0];
        const double n_out_f = std::floor(span / target_dt) + 1.0;
        if (n_out_f > 100'000'000.0)
        {
            PyErr_SetString(PyExc_ValueError,
                "resample: output would exceed 100M samples, target_dt is too small");
            return nullptr;
        }
    }

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
            const double n_out_f = std::floor((x_end - x_start) / dt) + 1.0;
            if (n_out_f <= 0.0 || n_out_f > 100'000'000.0)
            {
                PyErr_SetString(PyExc_ValueError,
                    "resample: output would exceed 100M samples, dt is too small");
                return static_cast<PyObject*>(nullptr);
            }
            const auto n_out = static_cast<npy_intp>(n_out_f);
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

            SQDSP_GIL_RELEASE_BEGIN
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
            SQDSP_GIL_RELEASE_END

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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;

    if (y.dtype != coeffs.dtype)
    {
        PyErr_SetString(PyExc_TypeError, "y and coeffs must have the same dtype");
        return nullptr;
    }
    if (coeffs.nrows < 1)
    {
        PyErr_SetString(PyExc_ValueError, "coeffs must have at least 1 tap");
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::fir_apply_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    coeffs.typed_data<T>(), static_cast<std::size_t>(coeffs.nrows), out.y_ptr);
            SQDSP_GIL_RELEASE_END
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
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
    if (!check_sos_a0(sos))
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::sos_apply_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    sos.typed_data<T>(), static_cast<std::size_t>(sos.nrows), out.y_ptr);
            SQDSP_GIL_RELEASE_END
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;

    if (y.dtype != coeffs.dtype)
    {
        PyErr_SetString(PyExc_TypeError, "y and coeffs must have the same dtype");
        return nullptr;
    }
    if (coeffs.nrows < 1)
    {
        PyErr_SetString(PyExc_ValueError, "coeffs must have at least 1 tap");
        return nullptr;
    }
    if (!check_min_rows(x, 2, "filtfilt"))
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::filtfilt_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    coeffs.typed_data<T>(), static_cast<std::size_t>(coeffs.nrows), out.y_ptr);
            SQDSP_GIL_RELEASE_END
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
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
    if (!check_sos_a0(sos))
        return nullptr;
    if (!check_min_rows(x, 2, "sosfiltfilt"))
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::sosfiltfilt_column(
                    y.typed_data<T>(), nrows, ncols, col,
                    sos.typed_data<T>(), static_cast<std::size_t>(sos.nrows), out.y_ptr);
            SQDSP_GIL_RELEASE_END
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;
    if (!check_min_rows(x, 2, "fft"))
        return nullptr;

    const auto win = parse_name(window_str, window_names, "window");
    if (!win)
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<sqp::dsp::FFTResult<T>> results;
        SQDSP_GIL_RELEASE_BEGIN
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
        results = sqp::dsp::fft(segments, *win);
        SQDSP_GIL_RELEASE_END

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
            PyList_SetItem(list, static_cast<Py_ssize_t>(i), tuple);
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
    PyObject* overlap_obj = Py_None;
    double gap_factor = 3.0;
    const char* window_str = "hann";

    int has_gaps = 1;

    static const char* kwlist[]
        = { "x", "y", "col", "window_size", "overlap", "gap_factor", "window", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|nnOdsp", const_cast<char**>(kwlist),
            &x_obj, &y_obj, &col, &window_size, &overlap_obj, &gap_factor, &window_str, &has_gaps))
        return nullptr;
    const Py_ssize_t overlap
        = (overlap_obj == Py_None) ? window_size / 2 : PyNumber_AsSsize_t(overlap_obj, nullptr);
    if (overlap == -1 && PyErr_Occurred())
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;
    if (col < 0 || col >= y.ncols)
    {
        PyErr_Format(PyExc_ValueError,
            "spectrogram: col (%zd) out of range [0, %zd)", col, y.ncols);
        return nullptr;
    }
    if (window_size < 2)
    {
        PyErr_SetString(PyExc_ValueError, "spectrogram: window_size must be >= 2");
        return nullptr;
    }
    if (overlap < 0 || overlap >= window_size)
    {
        PyErr_Format(PyExc_ValueError,
            "spectrogram: overlap (%zd) must be in [0, window_size=%zd)", overlap, window_size);
        return nullptr;
    }

    const auto win = parse_name(window_str, window_names, "window");
    if (!win)
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<sqp::dsp::SpectrogramResult<T>> results;
        SQDSP_GIL_RELEASE_BEGIN
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
            static_cast<std::size_t>(window_size), static_cast<std::size_t>(overlap), *win);
        SQDSP_GIL_RELEASE_END

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
            PyList_SetItem(list, static_cast<Py_ssize_t>(i), tuple);
        }
        return list;
    });
}

PyObject* dsp_column_percentile(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* y_obj = nullptr;
    double q = 50.0;

    static const char* kwlist[] = { "y", "q", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "O|d", const_cast<char**>(kwlist), &y_obj, &q))
        return nullptr;

    YArray y;
    if (!y.parse(y_obj))
        return nullptr;
    if (!check_percentile(q, "column_percentile"))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<T> vals;
        SQDSP_GIL_RELEASE_BEGIN
        vals = sqp::dsp::column_percentiles<T>(y.typed_data<T>(),
            static_cast<std::size_t>(y.nrows), static_cast<std::size_t>(y.ncols), q);
        SQDSP_GIL_RELEASE_END
        return vec_to_1d(vals);
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;
    if (!check_window_size(window, y.nrows, "rolling_mean"))
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::rolling_mean_column(
                    y.typed_data<T>(), nrows, ncols, col, win, out.y_ptr);
            SQDSP_GIL_RELEASE_END
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;
    if (!check_window_size(window, y.nrows, "rolling_std"))
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::rolling_std_column(
                    y.typed_data<T>(), nrows, ncols, col, win, out.y_ptr);
            SQDSP_GIL_RELEASE_END
            return out.to_tuple();
        }
        auto stage = sqp::dsp::rolling_std<T>(static_cast<std::size_t>(window));
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}

PyObject* dsp_rolling_percentile(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t window = 51;
    double q = 50.0;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[]
        = { "x", "y", "window", "q", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOn|ddp", const_cast<char**>(kwlist),
            &x_obj, &y_obj, &window, &q, &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;
    if (!check_window_size(window, y.nrows, "rolling_percentile"))
        return nullptr;
    if (!check_percentile(q, "rolling_percentile"))
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::rolling_percentile_column(
                    y.typed_data<T>(), nrows, ncols, col, win, q, out.y_ptr);
            SQDSP_GIL_RELEASE_END
            return out.to_tuple();
        }
        auto stage = sqp::dsp::rolling_percentile<T>(static_cast<std::size_t>(window), q);
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
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;
    const auto op = parse_name(op_str, reduce_op_names, "op");
    if (!op)
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
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t row = 0; row < nrows; ++row)
                out.y_ptr[row] = sqp::dsp::detail::reduce_row(
                    &y.typed_data<T>()[row * ncols], ncols, *op);
            SQDSP_GIL_RELEASE_END
            return out.to_tuple();
        }
        auto stage = sqp::dsp::reduce<T>(*op);
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
    if (!check_xy_sizes(x, y))
        return nullptr;

    const auto dims = as_long_vector(shape_obj, "shape");
    if (!dims)
        return nullptr;
    const std::size_t ndim = dims->size();
    std::vector<std::size_t> shape(ndim);
    std::size_t prod = 1;
    for (std::size_t i = 0; i < ndim; ++i)
    {
        const long dim = (*dims)[i];
        if (dim <= 0)
        {
            PyErr_Format(PyExc_ValueError, "shape dims must be positive, got %ld at index %zu",
                         dim, i);
            return nullptr;
        }
        shape[i] = static_cast<std::size_t>(dim);
        if (prod > std::numeric_limits<std::size_t>::max() / shape[i])
        {
            PyErr_SetString(PyExc_ValueError, "prod(shape) overflows");
            return nullptr;
        }
        prod *= shape[i];
    }

    if (static_cast<npy_intp>(prod) != y.ncols)
    {
        PyErr_Format(PyExc_ValueError,
            "prod(shape) = %zu does not match n_cols = %zd", prod, y.ncols);
        return nullptr;
    }

    const auto requested_axes = as_long_vector(axes_obj, "axes");
    if (!requested_axes)
        return nullptr;
    const std::size_t n_axes = requested_axes->size();
    std::vector<std::size_t> axes(n_axes);
    for (std::size_t i = 0; i < n_axes; ++i)
    {
        long axis = (*requested_axes)[i];
        if (axis < 0)
            axis += static_cast<long>(ndim); // numpy-style negative indexing
        if (axis < 0 || static_cast<std::size_t>(axis) >= ndim)
        {
            PyErr_Format(PyExc_ValueError, "axis %ld out of range for ndim = %zu", axis, ndim);
            return nullptr;
        }
        axes[i] = static_cast<std::size_t>(axis);
    }

    const auto op = parse_name(op_str, reduce_op_names, "op");
    if (!op)
        return nullptr;

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

        SQDSP_GIL_RELEASE_BEGIN
        sqp::dsp::parallel_for(nrows, [&](std::size_t row) {
            sqp::dsp::detail::reduce_axes_row(
                &y.typed_data<T>()[row * prod], spec, *op, &y_out_ptr[row * spec.out_size]);
        });
        SQDSP_GIL_RELEASE_END

        auto* tuple = PyTuple_Pack(2, x_ref, y_out_obj);
        Py_DECREF(x_ref);
        Py_DECREF(y_out_obj);
        return tuple;
    });
}

// ── Module definition ────────────────────────────────────────────────────────

// Docstrings start with a text signature ("name($module, ...)\n--\n\n"): it gives
// inspect.signature, help() and Sphinx the real parameters of these C functions.
#define SQDSP_DOC_X                                                                            \
    "x : array_like, shape (n,)\n"                                                             \
    "    Sample times, in seconds, increasing. Converted to float64.\n"
#define SQDSP_DOC_Y                                                                            \
    "y : array_like, shape (n,) or (n, k)\n"                                                   \
    "    Values: one row per time, one column per component.\n"                                \
    "    float64, float32 and int32 are kept; other integer and float types are\n"     \
    "    computed as float64.\n"
#define SQDSP_DOC_GAPS                                                                         \
    "gap_factor : float, default 3.0\n"                                                        \
    "    A step longer than gap_factor times the median step of x is a gap. Must be > 0.\n"    \
    "has_gaps : bool, default True\n"                                                          \
    "    False skips the gap search and treats the data as one segment: faster, and no\n"     \
    "    gap rows are added. Use it for regularly sampled data.\n"
#define SQDSP_DOC_TIME_SERIES_RETURN                                                           \
    "Returns\n"                                                                                \
    "-------\n"                                                                                \
    "x_out : ndarray of float64\n"                                                             \
    "    The times, plus one row at the middle of each gap.\n"                                 \
    "y_out : ndarray\n"                                                                        \
    "    Same type and number of columns as y. Each segment is processed on its own,\n"      \
    "    and gap rows hold NaN (0 for int32).\n"
#define SQDSP_DOC_WINDOW                                                                       \
    "window : {'hann', 'hamming', 'blackman', 'flattop', 'rectangular'}, default 'hann'\n"   \
    "    Taper applied before the transform. Other names raise ValueError.\n"
#define SQDSP_DOC_OP                                                                           \
    "op : {'sum', 'mean', 'min', 'max', 'norm'}\n"                                            \
    "    norm is the square root of the sum of squares. NaN values are skipped. Only\n"      \
    "    NaN gives NaN for mean, min and max, 0 for sum and norm, like np.nansum.\n"      \
    "    Other names raise ValueError.\n"

// clang-format off
PyMethodDef methods[] = {
    {"split_segments", reinterpret_cast<PyCFunction>(dsp_split_segments),
     METH_VARARGS | METH_KEYWORDS,
     "split_segments($module, x, y, gap_factor=3.0)\n--\n\n"
     "Find the gaps in x and return the row ranges between them.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "gap_factor : float, default 3.0\n"
     "    A step longer than gap_factor times the median step of x is a gap. Must be > 0.\n\n"
     "Returns\n-------\n"
     "list of (int, int)\n"
     "    One (start, end) pair per segment, end excluded: y[start:end].\n"},

    {"interpolate_nan", reinterpret_cast<PyCFunction>(dsp_interpolate_nan),
     METH_VARARGS | METH_KEYWORDS,
     "interpolate_nan($module, x, y, max_consecutive=1)\n--\n\n"
     "Fill short runs of NaN by linear interpolation in x.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "max_consecutive : int, default 1\n"
     "    Longest run of NaN to fill. Longer runs, and runs at either end, stay NaN.\n\n"
     "Returns\n-------\n"
     "ndarray\n"
     "    A copy of y, same type and shape. int32 data is returned unchanged.\n"},

    {"resample", reinterpret_cast<PyCFunction>(dsp_resample),
     METH_VARARGS | METH_KEYWORDS,
     "resample($module, x, y, gap_factor=3.0, target_dt=0.0, has_gaps=True)\n--\n\n"
     "Put each segment on a regular time grid, by linear interpolation.\n\n"
     "Each segment's grid starts at its first sample.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y SQDSP_DOC_GAPS
     "target_dt : float, default 0.0\n"
     "    Grid step, in seconds. 0 uses the median step of x. A grid of more than\n"
     "    100 million samples raises ValueError.\n\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"fir_filter", reinterpret_cast<PyCFunction>(dsp_fir_filter),
     METH_VARARGS | METH_KEYWORDS,
     "fir_filter($module, x, y, coeffs, gap_factor=3.0, has_gaps=True)\n--\n\n"
     "Apply a causal FIR filter to each segment.\n\n"
     "The output is delayed by (len(coeffs) - 1) / 2 samples for a symmetric filter.\n"
     "Use filtfilt for a filter without delay.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "coeffs : array_like, shape (n_taps,)\n"
     "    Filter taps. Must have the same type as y, else TypeError.\n"
     SQDSP_DOC_GAPS "\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"iir_sos", reinterpret_cast<PyCFunction>(dsp_iir_sos),
     METH_VARARGS | METH_KEYWORDS,
     "iir_sos($module, x, y, sos, gap_factor=3.0, has_gaps=True)\n--\n\n"
     "Apply a causal IIR filter, given as second-order sections, to each segment.\n\n"
     "Use sosfiltfilt for a filter without delay.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "sos : array_like, shape (n_sections, 6)\n"
     "    One row [b0, b1, b2, a0, a1, a2] per section, as scipy.signal.butter(...,\n"
     "    output='sos') returns. Must have the same type as y, else TypeError.\n"
     SQDSP_DOC_GAPS "\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"filtfilt", reinterpret_cast<PyCFunction>(dsp_filtfilt),
     METH_VARARGS | METH_KEYWORDS,
     "filtfilt($module, x, y, coeffs, gap_factor=3.0, has_gaps=True)\n--\n\n"
     "Apply a FIR filter forward then backward: zero phase, no delay.\n\n"
     "Matches scipy.signal.filtfilt (odd padding), edges included.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "coeffs : array_like, shape (n_taps,)\n"
     "    Filter taps. Must have the same type as y, else TypeError.\n"
     SQDSP_DOC_GAPS "\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"sosfiltfilt", reinterpret_cast<PyCFunction>(dsp_sosfiltfilt),
     METH_VARARGS | METH_KEYWORDS,
     "sosfiltfilt($module, x, y, sos, gap_factor=3.0, has_gaps=True)\n--\n\n"
     "Apply an IIR filter forward then backward: zero phase, no delay.\n\n"
     "Matches scipy.signal.sosfiltfilt, edges included.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "sos : array_like, shape (n_sections, 6)\n"
     "    One row [b0, b1, b2, a0, a1, a2] per section, as scipy.signal.butter(...,\n"
     "    output='sos') returns. Must have the same type as y, else TypeError.\n"
     SQDSP_DOC_GAPS "\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"fft", reinterpret_cast<PyCFunction>(dsp_fft),
     METH_VARARGS | METH_KEYWORDS,
     "fft($module, x, y, gap_factor=3.0, window='hann', has_gaps=True)\n--\n\n"
     "Magnitude spectrum of each segment.\n\n"
     "The magnitude is ``|FFT| / N``, one-sided: a sine of amplitude A peaks at A/2 with\n"
     "the rectangular window, A/4 with Hann. The sampling rate is 1 / median step of x,\n"
     "so frequencies are in hertz when x is in seconds.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y SQDSP_DOC_GAPS SQDSP_DOC_WINDOW "\n"
     "Returns\n-------\n"
     "list of (freqs, magnitude)\n"
     "    One pair per segment. freqs is float64, shape (N // 2 + 1,). magnitude has\n"
     "    y's type, one row per frequency and y's columns.\n"},

    {"spectrogram", reinterpret_cast<PyCFunction>(dsp_spectrogram),
     METH_VARARGS | METH_KEYWORDS,
     "spectrogram($module, x, y, col=0, window_size=256, overlap=None, gap_factor=3.0, "
     "window='hann', has_gaps=True)\n--\n\n"
     "Power spectra of sliding windows over one column, for each segment.\n\n"
     "Each window gives ``|FFT / N|^2``, stamped at the time of its centre sample.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "col : int, default 0\n"
     "    Column of y to analyse.\n"
     "window_size : int, default 256\n"
     "    Samples per window. At least 2. Segments shorter than this give empty arrays.\n"
     "overlap : int or None, default None\n"
     "    Samples shared by neighbouring windows, from 0 to window_size - 1. None means\n"
     "    half a window.\n"
     SQDSP_DOC_GAPS SQDSP_DOC_WINDOW "\n"
     "Returns\n-------\n"
     "list of (t, f, power)\n"
     "    One triple per segment. t and f are float64. power has y's type and shape\n"
     "    (len(t), len(f)): what a colour map plot takes.\n"},

    {"column_percentile", reinterpret_cast<PyCFunction>(dsp_column_percentile),
     METH_VARARGS | METH_KEYWORDS,
     "column_percentile($module, y, q=50.0)\n--\n\n"
     "Percentile of each column, down the time axis.\n\n"
     "Matches np.nanpercentile(y, q, axis=0): NaN values are skipped, a column of only\n"
     "NaN gives NaN. A quick background estimate for a spectrogram.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_Y
     "q : float, default 50.0\n"
     "    Percentile, from 0 to 100. 50 is the median.\n\n"
     "Returns\n-------\n"
     "ndarray, shape (k,)\n"
     "    One value per column, y's type.\n"},

    {"rolling_mean", reinterpret_cast<PyCFunction>(dsp_rolling_mean),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_mean($module, x, y, window, gap_factor=3.0, has_gaps=True)\n--\n\n"
     "Mean over a sliding window centred on each sample.\n\n"
     "The window shrinks near the ends of each segment and never crosses a gap. NaN\n"
     "samples are skipped; a window of only NaN gives NaN (0 for int32).\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "window : int\n"
     "    Window length in samples, not seconds. From 1 to len(x).\n"
     SQDSP_DOC_GAPS "\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"rolling_std", reinterpret_cast<PyCFunction>(dsp_rolling_std),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_std($module, x, y, window, gap_factor=3.0, has_gaps=True)\n--\n\n"
     "Sample standard deviation (divided by n - 1) over a sliding window centred on\n"
     "each sample.\n\n"
     "The window shrinks near the ends of each segment and never crosses a gap. NaN\n"
     "samples are skipped; a window with fewer than two values gives NaN (0 for int32).\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "window : int\n"
     "    Window length in samples, not seconds. From 1 to len(x).\n"
     SQDSP_DOC_GAPS "\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"rolling_percentile", reinterpret_cast<PyCFunction>(dsp_rolling_percentile),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_percentile($module, x, y, window, q=50.0, gap_factor=3.0, has_gaps=True)\n"
     "--\n\n"
     "Percentile over a sliding window centred on each sample. q=50 is a rolling median.\n\n"
     "The window shrinks near the ends of each segment and never crosses a gap. NaN\n"
     "samples are skipped; a window of only NaN gives NaN.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "window : int\n"
     "    Window length in samples, not seconds. From 1 to len(x).\n"
     "q : float, default 50.0\n"
     "    Percentile, from 0 to 100.\n"
     SQDSP_DOC_GAPS "\n"
     SQDSP_DOC_TIME_SERIES_RETURN},

    {"reduce", reinterpret_cast<PyCFunction>(dsp_reduce),
     METH_VARARGS | METH_KEYWORDS,
     "reduce($module, x, y, op, gap_factor=3.0, has_gaps=True)\n--\n\n"
     "Combine the columns of each row into one value.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y SQDSP_DOC_OP SQDSP_DOC_GAPS "\n"
     "Returns\n-------\n"
     "x_out : ndarray of float64\n"
     "    The times, plus one row at the middle of each gap.\n"
     "y_out : ndarray, shape (len(x_out),)\n"
     "    y's type. Gap rows hold NaN (0 for int32).\n"},

    {"reduce_axes", reinterpret_cast<PyCFunction>(dsp_reduce_axes),
     METH_VARARGS | METH_KEYWORDS,
     "reduce_axes($module, x, y, shape, axes, op='sum', has_gaps=True)\n--\n\n"
     "Reduce some of the dimensions packed in the columns of each row.\n\n"
     "Example: 32 energies x 16 x 8 angles stored as 4096 columns; shape=(32, 16, 8)\n"
     "and axes=(1, 2) sum over the angles and leave 32 columns, one per energy.\n"
     "Columns are read in C order: the last axis changes fastest. No gap rows are\n"
     "added, and has_gaps has no effect.\n\n"
     "Parameters\n----------\n"
     SQDSP_DOC_X SQDSP_DOC_Y
     "shape : sequence of int\n"
     "    The dimensions of one row. Their product must equal the number of columns.\n"
     "axes : sequence of int\n"
     "    The dimensions to reduce. Negative values count from the end.\n"
     SQDSP_DOC_OP
     "has_gaps : bool, default True\n"
     "    Ignored.\n\n"
     "Returns\n-------\n"
     "x_out : ndarray of float64\n"
     "    x, unchanged.\n"
     "y_out : ndarray\n"
     "    y's type, one column per combination of the kept dimensions; 1-D when\n"
     "    only one is left.\n"},

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

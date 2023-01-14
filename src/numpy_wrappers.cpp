/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/numpy_wrappers.hpp"


#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#if defined(slots) && (defined(__GNUC__) || defined(_MSC_VER) || defined(__clang__))
#pragma push_macro("slots")
#undef slots
extern "C"
{
/*
 * Python 2 uses the "register" keyword, which is deprecated in C++ 11
 * and forbidden in C++17.
 */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-register"
#endif

#include <Python.h>
#include <numpy/arrayobject.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}
#else
extern "C"
{
#include <Python.h>
#include <numpy/arrayobject.h>
}
#endif

inline int init_numpy()
{
    DISABLE_WARNING_PUSH
    DISABLE_WARNING_CONVERSION_NULL
    import_array(); // PyError if not successful
    if (PyErr_Occurred())
    {
        std::cerr << "Failed to import numpy Python module(s)." << std::endl;
        return -1; // Or some suitable return value to indicate failure.
    }
    DISABLE_WARNING_POP
    return 0;
}
const static int numpy_initialized = init_numpy();

bool NpArray_view::isNpArray(PyObject* obj)
{
    auto arr = reinterpret_cast<PyArrayObject*>(obj);
    auto is_c_aray = obj && PyArray_Check(arr) && PyArray_ISCARRAY(arr);
    return is_c_aray;
}

NpArray_view::NpArray_view(PyObject* obj) : _py_obj { obj }
{
    assert(isNpArray(obj));
    assert(PyArray_ISFLOAT(_py_obj.get()));
}

std::vector<std::size_t> NpArray_view::shape() const
{
    std::vector<std::size_t> shape;
    if (!_py_obj.is_null())
    {
        if (int ndim = PyArray_NDIM(_py_obj.get()); ndim > 0)
        {
            if (ndim < 10)
            {
                shape.resize(ndim);
                std::copy_n(PyArray_SHAPE(_py_obj.get()), ndim, std::begin(shape));
            }
        }
    }
    return shape;
}

std::size_t NpArray_view::ndim()
{
    if (!_py_obj.is_null())
    {
        return static_cast<std::size_t>(PyArray_NDIM(_py_obj.get()));
    }
    return 0;
}

std::size_t NpArray_view::size(std::size_t index)
{
    if (!_py_obj.is_null())
    {
        if (index < static_cast<std::size_t>(PyArray_NDIM(_py_obj.get())))
        {
            return PyArray_SHAPE(_py_obj.get())[index];
        }
    }
    return 0;
}

double* NpArray_view::data() const
{
    if (!_py_obj.is_null())
    {
        return reinterpret_cast<double*>(PyArray_DATA(_py_obj.get()));
    }
    return nullptr;
}

std::vector<double> NpArray_view::to_std_vect()
{
    assert(!this->_py_obj.is_null());
    auto sz = flat_size();
    std::vector<double> v(sz);
    auto d_ptr = reinterpret_cast<double*>(PyArray_DATA(_py_obj.get()));
    std::copy(d_ptr, d_ptr + sz, std::begin(v));
    return v;
}

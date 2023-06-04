/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#pragma once
/*#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#ifdef NO_NO_IMPORT_ARRAY
#else
#endif
#define PY_ARRAY_UNIQUE_SYMBOL SCIQLOPPLOTSBINDINGS_ARRAY_API*/


extern "C"
{
#include <Python.h>
}
#include <algorithm>
#include <assert.h>
#include <cpp_utils/warnings.h>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <vector>


template <typename dest_type = PyObject>
struct PyObjectWrapper
{
private:
    PyObject* _py_obj = nullptr;
    void inc_refcount()
    {
        PyGILState_STATE state = PyGILState_Ensure();
        Py_XINCREF(_py_obj);
        PyGILState_Release(state);
    }
    void dec_refcount()
    {
        PyGILState_STATE state = PyGILState_Ensure();
        Py_XDECREF(_py_obj);
        PyGILState_Release(state);
        _py_obj = nullptr;
    }

public:
    PyObjectWrapper() : _py_obj { nullptr } { }
    PyObjectWrapper(const PyObjectWrapper& other) : _py_obj { other._py_obj } { inc_refcount(); }
    PyObjectWrapper(PyObjectWrapper&& other) : _py_obj { other._py_obj } { inc_refcount(); }
    explicit PyObjectWrapper(PyObject* obj) : _py_obj { obj } { inc_refcount(); }
    ~PyObjectWrapper() { dec_refcount(); }
    PyObjectWrapper& operator=(PyObjectWrapper&& other)
    {
        dec_refcount();
        this->_py_obj = other._py_obj;
        inc_refcount();
        return *this;
    }
    PyObjectWrapper& operator=(const PyObjectWrapper& other)
    {
        dec_refcount();
        this->_py_obj = other._py_obj;
        inc_refcount();
        return *this;
    }

    PyObject* py_object() { return _py_obj; }
    inline dest_type* get() { return reinterpret_cast<dest_type*>(_py_obj); }
    inline dest_type* get() const { return reinterpret_cast<dest_type*>(_py_obj); }
    inline bool is_null() const { return _py_obj == nullptr; }
};

struct Array_view
{
private:
    PyObjectWrapper<PyObject> _py_obj;
    Py_buffer _buffer = { 0 };
    std::vector<Py_ssize_t> _shape;
    bool _is_valid = false;
    Array_view(const Array_view&& other) = delete;
    void _init_buffer();

public:
    using value_type = double;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    Array_view() : _py_obj { nullptr } { }
    Array_view(const Array_view& other) : _py_obj { other._py_obj } { this->_init_buffer(); }
    Array_view(Array_view&& other) : _py_obj { other._py_obj } { this->_init_buffer(); }
    explicit Array_view(PyObject* obj);

    ~Array_view();

    inline Array_view& operator=(const Array_view& other)
    {
        this->_py_obj = other._py_obj;
        this->_init_buffer();
        return *this;
    }

    inline Array_view& operator=(Array_view&& other)
    {
        this->_py_obj = other._py_obj;
        this->_init_buffer();
        return *this;
    }

    std::vector<std::size_t> shape() const;

    std::size_t ndim();

    std::size_t size(std::size_t index = 0);

    inline std::size_t flat_size() const
    {
        if (this->_is_valid)
            return static_cast<std::size_t>(this->_buffer.len / this->_buffer.itemsize);
        return 0UL;
    }

    double* data() const;

    inline auto operator[](std::size_t position) { return data()[position]; }
    inline const auto operator[](std::size_t position) const { return data()[position]; }

    std::vector<double> to_std_vect();

    inline PyObject* py_object() { return _py_obj.py_object(); }

    inline auto begin() noexcept { return data(); }

    inline auto end() noexcept { return data() + flat_size(); }

    inline const auto begin() const noexcept { return data(); }

    inline const auto end() const noexcept { return data() + flat_size(); }

    inline const auto cbegin() const noexcept { return data(); }

    inline const auto cend() const noexcept { return data() + flat_size(); }

    inline auto front() { return *begin(); }
    inline auto back() { return *(end() - 1); }
};

namespace std
{

inline auto size(const Array_view& v)
{
    return v.flat_size();
}

inline auto cbegin(const Array_view& v)
{
    return v.cbegin();
}

inline auto cend(const Array_view& v)
{
    return v.cend();
}

}

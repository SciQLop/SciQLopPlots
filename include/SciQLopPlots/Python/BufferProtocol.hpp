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

struct PyObjectWrapper
{
private:
    PyObject* _py_obj = nullptr;

    inline void set_obj(PyObject* py_obj)
    {
        this->release_obj();

        if (py_obj != nullptr)
        {
            PyGILState_STATE state = PyGILState_Ensure();
            Py_INCREF(_py_obj);
            PyGILState_Release(state);
        }
    }

    inline void release_obj()
    {
        if (_py_obj != nullptr)
        {
            PyGILState_STATE state = PyGILState_Ensure();
            Py_DECREF(_py_obj);
            PyGILState_Release(state);
        }
    }

    inline void take(PyObjectWrapper& other)
    {
        this->set_obj(other.py_object());
        other.release_obj();
    }

    inline void borrow(const PyObjectWrapper& other) { this->set_obj(other.py_object()); }

public:
    PyObjectWrapper() : _py_obj { nullptr } { }
    PyObjectWrapper(const PyObjectWrapper& other) { this->borrow(other); }
    PyObjectWrapper(PyObjectWrapper&& other) { this->take(other); }
    explicit PyObjectWrapper(PyObject* obj) { this->set_obj(obj); }
    ~PyObjectWrapper();
    PyObjectWrapper& operator=(PyObjectWrapper&& other)
    {
        this->take(other);
        return *this;
    }
    PyObjectWrapper& operator=(const PyObjectWrapper& other)
    {
        this->borrow(other);
        return *this;
    }


    inline PyObject* py_object() const { return _py_obj; }
    inline bool is_null() const { return _py_obj == nullptr; }
};

struct Array_view
{
private:
    // PyObjectWrapper _py_obj;
    PyObject* _py_obj = nullptr;
    Py_buffer _buffer = { 0 };
    std::vector<Py_ssize_t> _shape;
    bool _is_valid = false;
    Array_view(const Array_view&& other) = delete;
    void _init_buffer(PyObject* obj);

    inline void steal(Array_view&& other)
    {
        this->_py_obj = other._py_obj;
        this->_buffer = other._buffer;
        this->_is_valid = other._is_valid;
        other._is_valid = false;
        this->_shape = other._shape;
    }

    inline void share(const Array_view& other) { this->_init_buffer(other._py_obj); }

public:
    using value_type = double;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    Array_view() { }
    Array_view(const Array_view& other)
    {
        if (other._is_valid)
            this->share(other);
    }
    Array_view(Array_view&& other) { this->steal(std::move(other)); }
    explicit Array_view(PyObject* obj);

    ~Array_view();

    inline Array_view& operator=(const Array_view& other)
    {
        if (other._is_valid)
            this->share(other);
        return *this;
    }

    inline Array_view& operator=(Array_view&& other)
    {
        this->steal(std::move(other));
        return *this;
    }

    inline void release()
    {
        if (this->_is_valid)
        {
            // std::cout << "Array_view PyBuffer_Release" << std::endl;
            PyGILState_STATE state = PyGILState_Ensure();
            PyBuffer_Release(&this->_buffer);
            PyGILState_Release(state);
            this->_is_valid = false;
            this->_buffer = { 0 };
            this->_py_obj = nullptr;
        }
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


    inline auto begin() noexcept { return data(); }

    inline auto end() noexcept { return data() + flat_size(); }

    inline const auto begin() const noexcept { return data(); }

    inline const auto end() const noexcept { return data() + flat_size(); }

    inline const auto cbegin() const noexcept { return data(); }

    inline const auto cend() const noexcept { return data() + flat_size(); }

    inline auto front() { return *begin(); }
    inline auto back() { return *(end() - 1); }

    inline PyObject* py_object() const { return _py_obj; }
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

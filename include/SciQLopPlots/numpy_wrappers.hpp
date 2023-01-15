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
#pragma once
extern "C"
{
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
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
    void inc_refcount() { Py_XINCREF(_py_obj); }
    void dec_refcount()
    {
        Py_XDECREF(_py_obj);
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

struct NpArray_view
{
private:
    PyObjectWrapper<PyArrayObject> _py_obj;
    NpArray_view(const NpArray_view&& other) = delete;

public:
    using value_type = double;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    static bool isNpArray(PyObject* obj);
    NpArray_view() : _py_obj { nullptr } { }
    NpArray_view(const NpArray_view& other) : _py_obj { other._py_obj } { }
    NpArray_view(NpArray_view&& other) : _py_obj { other._py_obj } { }
    explicit NpArray_view(PyObject* obj);

    ~NpArray_view() { }

    inline NpArray_view& operator=(const NpArray_view& other)
    {
        this->_py_obj = other._py_obj;
        return *this;
    }

    inline NpArray_view& operator=(NpArray_view&& other)
    {
        this->_py_obj = other._py_obj;
        return *this;
    }

    std::vector<std::size_t> shape() const;

    std::size_t ndim();

    std::size_t size(std::size_t index = 0);

    inline std::size_t flat_size() const
    {
        auto s = this->shape();
        return std::accumulate(
            std::cbegin(s), std::cend(s), 1, [](const auto& a, const auto& b) { return a * b; });
    }

    double* data() const;

    inline auto operator[](std::size_t position) { return data()[position]; }
    inline const auto operator[](std::size_t position) const { return data()[position]; }

    std::vector<double> to_std_vect();

    inline PyObject* py_object() { return _py_obj.py_object(); }

    inline auto begin() noexcept { return data(); }

    inline auto end() noexcept { return data() + flat_size(); }

    inline const auto cbegin() const noexcept { return data(); }

    inline const auto cend() const noexcept { return data() + flat_size(); }

    inline auto front() { return *begin(); }
    inline auto back() { return *(end() - 1); }
};

namespace std
{

inline auto size(const NpArray_view& v)
{
    return v.flat_size();
}

inline auto cbegin(const NpArray_view& v)
{
    return v.cbegin();
}

inline auto cend(const NpArray_view& v)
{
    return v.cend();
}

}

struct NpArray
{
    std::vector<std::size_t> shape;
    std::vector<double> data;
    inline static bool isNpArray(PyObject* obj) { return NpArray_view::isNpArray(obj); }
    NpArray() = default;
    explicit NpArray(PyObject* obj)
    {
        if (obj)
        {
            NpArray_view view { obj };
            shape = view.shape();
            data = view.to_std_vect();
        }
    }

    inline std::size_t ndim() { return shape.size(); }

    inline std::size_t size(std::size_t index = 0)
    {
        if (index < shape.size())
            return shape[index];
        return 0;
    }

    inline std::size_t flat_size()
    {
        return std::accumulate(std::cbegin(shape), std::cend(shape), 1,
            [](const auto& a, const auto& b) { return a * b; });
    }

    // TODO maybe ;)
    PyObject* py_object() { return nullptr; }
};

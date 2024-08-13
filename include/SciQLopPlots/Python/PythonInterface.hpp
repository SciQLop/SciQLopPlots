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


#include <assert.h>
#include <cpp_utils/warnings.h>

#include <iterator>
#include <map>
#include <memory>
#include <vector>

extern "C"
{
#ifndef SKIP_PYTHON_INTERFACE_CPP
#ifndef PyObject_HEAD
    struct _object;
    typedef _object PyObject;
#endif

#endif
}

struct _Array_view_impl;

struct Array_view
{
private:
    std::shared_ptr<_Array_view_impl> _impl = nullptr;


    void steal(Array_view&& other);

    void share(const Array_view& other);

public:
    using value_type = double;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    Array_view();
    Array_view(const Array_view& other);
    Array_view(Array_view&& other);
    explicit Array_view(PyObject* obj);

    ~Array_view();

    Array_view& operator=(const Array_view& other);
    Array_view& operator=(Array_view&& other);


    bool is_valid() const;


    void release();

    const std::vector<std::size_t>& shape() const;

    std::size_t ndim();

    std::size_t size(std::size_t index = 0);

    std::size_t flat_size() const;

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

    PyObject* py_object() const;
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

struct _GetDataPyCallable_impl;

struct GetDataPyCallable
{
private:
    _GetDataPyCallable_impl* _impl = nullptr;

    void steal(GetDataPyCallable& other);

    void share(const GetDataPyCallable& other);

public:
    explicit GetDataPyCallable(PyObject* obj);
    GetDataPyCallable();
    GetDataPyCallable(const GetDataPyCallable& other);
    GetDataPyCallable(GetDataPyCallable&& other);

    GetDataPyCallable& operator=(const GetDataPyCallable& other);
    GetDataPyCallable& operator=(GetDataPyCallable&& other);

    ~GetDataPyCallable();

    PyObject* py_object() const;
    bool is_valid() const;

    void release();

    std::vector<Array_view> get_data(double lower, double upper);
};

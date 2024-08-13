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

struct _PyBuffer_impl;

struct PyBuffer
{
private:
    std::shared_ptr<_PyBuffer_impl> _impl = nullptr;


    void share(const PyBuffer& other);

public:
    using value_type = double;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    PyBuffer();
    PyBuffer(const PyBuffer& other);
    PyBuffer(PyBuffer&& other);
    explicit PyBuffer(PyObject* obj);

    ~PyBuffer();

    PyBuffer& operator=(const PyBuffer& other);
    PyBuffer& operator=(PyBuffer&& other);


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

template <bool row_major = true>
struct ArrayView2D
{
    double* ptr;
    std::size_t n_rows;
    std::size_t n_cols;
    std::size_t offset;

    ArrayView2D(double* ptr, std::size_t n_rows, std::size_t n_cols, std::size_t offset = 0)
            : ptr(ptr), n_rows(n_rows), n_cols(n_cols), offset(offset)
    {
    }

    inline double operator[](std::pair<std::size_t, std::size_t> index) const
    {
        if constexpr (row_major)
        {
            return ptr[index.first * n_cols + index.second + offset];
        }
        else
        {
            return ptr[index.second * n_rows + index.first + offset];
        }
    }
};

namespace std
{

inline auto size(const PyBuffer& v)
{
    return v.flat_size();
}

inline auto cbegin(const PyBuffer& v)
{
    return v.cbegin();
}

inline auto cend(const PyBuffer& v)
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

    std::vector<PyBuffer> get_data(double lower, double upper);
};

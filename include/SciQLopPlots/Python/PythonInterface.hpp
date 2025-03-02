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
#include <cmath>
#include <cpp_utils/warnings.h>

#include <algorithm>
#include <iterator>
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

struct ArrayViewBase
{
    virtual ~ArrayViewBase() = default;
    virtual double operator[](std::pair<std::size_t, std::size_t> index) const = 0;
    virtual std::unique_ptr<ArrayViewBase> view(std::size_t first_row = 0,
                                                std::size_t last_row = 0) const
        = 0;
    virtual std::size_t flat_size() const noexcept = 0;
    virtual std::size_t size(int index = 0) const noexcept = 0;
};

struct ArrayView1D : public ArrayViewBase
{
private:
    double* ptr;
    std::size_t start; // first row
    std::size_t stop; // last row
    std::size_t _size;

public:
    ArrayView1D(double* ptr, std::size_t start, std::size_t stop)
            : ptr(ptr), start(start), stop(stop), _size(stop - start)
    {
    }

    inline double operator[](std::pair<std::size_t, std::size_t> index) const override
    {
        return ptr[index.first + start];
    }

    std::unique_ptr<ArrayViewBase> view(std::size_t first_row = 0,
                                        std::size_t last_row = 0) const override
    {
        if (last_row == 0)
            last_row = stop - start;
        return std::make_unique<ArrayView1D>(ptr, first_row + start, last_row + start);
    }

    inline std::size_t flat_size() const noexcept override { return _size; }

    inline std::size_t size(int index = 0) const noexcept override { return _size; }
};

template <bool row_major = true>
struct ArrayView2D : public ArrayViewBase
{
private:
    double* ptr;
    std::size_t n_rows;
    std::size_t n_cols;
    std::size_t start; // first row
    std::size_t stop; // last row
    std::size_t _flat_size;

public:
    ArrayView2D(double* ptr, std::size_t n_rows, std::size_t n_cols, std::size_t start,
                std::size_t stop)
            : ptr(ptr)
            , n_rows(n_rows)
            , n_cols(std::max(n_cols, std::size_t { 1UL }))
            , start(start)
            , stop(stop)
            , _flat_size((stop - start) * n_cols)
    {
    }

    inline double operator[](std::pair<std::size_t, std::size_t> index) const override
    {
        if constexpr (row_major)
        {
            return ptr[(index.first + start) * n_cols + index.second];
        }
        else
        {
            return ptr[index.first + start + (index.second * n_rows)];
        }
    }

    std::unique_ptr<ArrayViewBase> view(std::size_t first_row = 0,
                                        std::size_t last_row = 0) const override
    {
        if (last_row == 0)
            last_row = n_rows;
        return std::make_unique<ArrayView2D<row_major>>(ptr, n_rows, n_cols, first_row + start,
                                                        last_row + start);
    }

    inline std::size_t flat_size() const noexcept override { return _flat_size; }

    inline std::size_t size(int index = 0) const noexcept override
    {
        if (index == 0)
            return stop - start;
        return n_cols;
    }
};


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

    std::size_t ndim() const;

    std::size_t size(std::size_t index = 0) const;

    std::size_t flat_size() const;

    double* data() const;

    inline auto operator[](std::size_t position) { return data()[position]; }

    inline const auto operator[](std::size_t position) const { return data()[position]; }

    inline auto begin() noexcept { return data(); }

    inline auto end() noexcept { return data() + flat_size(); }

    inline const auto begin() const noexcept { return data(); }

    inline const auto end() const noexcept { return data() + flat_size(); }

    inline const auto cbegin() const noexcept { return data(); }

    inline const auto cend() const noexcept { return data() + flat_size(); }

    inline auto front() { return *begin(); }

    inline auto back() { return *(end() - 1); }

    bool row_major() const;

    inline std::unique_ptr<ArrayViewBase> view(std::size_t first_row = 0,
                                               std::size_t last_row = 0) const
    {
        if (last_row == 0)
            last_row = shape()[0];
        if (is_valid())
        {
            if (ndim() == 1)
                return std::make_unique<ArrayView1D>(data(), first_row, last_row);
            if (row_major())
                return std::make_unique<ArrayView2D<true>>(
                    data(), shape()[0], ndim() == 1 ? 0 : shape()[1], first_row, last_row);
            else
                return std::make_unique<ArrayView2D<false>>(
                    data(), shape()[0], ndim() == 1 ? 0 : shape()[1], first_row, last_row);
        }
        return nullptr;
    }

    PyObject* py_object() const;
};

namespace std
{

inline auto size(const PyBuffer& v)
{
    return v.flat_size();
}

inline auto size(const ArrayViewBase& v)
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
    std::vector<PyBuffer> get_data(PyBuffer x, PyBuffer y);
    std::vector<PyBuffer> get_data(PyBuffer x, PyBuffer y, PyBuffer z);
};

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
    virtual std::unique_ptr<ArrayViewBase> view(
        std::size_t first_row = 0, std::size_t last_row = 0) const
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

public:
    ArrayView1D(double* ptr, std::size_t start, std::size_t stop)
            : ptr(ptr), start(start), stop(stop)
    {
    }

    inline double operator[](std::pair<std::size_t, std::size_t> index) const override
    {
        return ptr[index.first + start];
    }

    std::unique_ptr<ArrayViewBase> view(
        std::size_t first_row = 0, std::size_t last_row = 0) const override
    {
        if (last_row == 0)
            last_row = stop - start;
        return std::make_unique<ArrayView1D>(ptr, first_row + start, last_row + start);
    }

    inline std::size_t flat_size() const noexcept override { return stop - start; }

    inline std::size_t size(int index = 0) const noexcept override { return stop - start; }
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

public:
    ArrayView2D(
        double* ptr, std::size_t n_rows, std::size_t n_cols, std::size_t start, std::size_t stop)
            : ptr(ptr)
            , n_rows(n_rows)
            , n_cols(std::max(n_cols, std::size_t { 1UL }))
            , start(start)
            , stop(stop)
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

    std::unique_ptr<ArrayViewBase> view(
        std::size_t first_row = 0, std::size_t last_row = 0) const override
    {
        if (last_row == 0)
            last_row = n_rows;
        return std::make_unique<ArrayView2D<row_major>>(
            ptr, n_rows, n_cols, first_row + start, last_row + start);
    }

    inline std::size_t flat_size() const noexcept override { return (stop - start) * n_cols; }

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

    std::vector<double> to_std_vect();

    inline auto begin() noexcept { return data(); }

    inline auto end() noexcept { return data() + flat_size(); }

    inline const auto begin() const noexcept { return data(); }

    inline const auto end() const noexcept { return data() + flat_size(); }

    inline const auto cbegin() const noexcept { return data(); }

    inline const auto cend() const noexcept { return data() + flat_size(); }

    inline auto front() { return *begin(); }

    inline auto back() { return *(end() - 1); }

    bool row_major() const;

    inline std::unique_ptr<ArrayViewBase> view(
        std::size_t first_row = 0, std::size_t last_row = 0) const
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

struct XYView
{
private:
    std::unique_ptr<ArrayViewBase> _x;
    std::unique_ptr<ArrayViewBase> _y;

public:
    explicit XYView(
        const PyBuffer& x, const PyBuffer& y, std::size_t start = 0, std::size_t stop = 0)
    {
        this->_x = x.view(start, stop);
        this->_y = y.view(start, stop);
    }

    explicit XYView(const PyBuffer& x, const PyBuffer& y, double x_start, double x_stop)
    {
        std::size_t start_index = std::distance(
            x.data(), std::upper_bound(x.data(), x.data() + x.flat_size(), x_start));
        std::size_t stop_index
            = std::distance(x.data(), std::lower_bound(x.data(), x.data() + x.flat_size(), x_stop));
        this->_x = x.view(start_index, stop_index);
        this->_y = y.view(start_index, stop_index);
    }

    inline double x(std::size_t i) const { return (*_x)[{ i, 0 }]; }

    inline double y(std::size_t i, std::size_t j) const { return (*_y)[{ i, j }]; }

    inline std::size_t size() const { return std::size(*_x); }
};

struct XYZView
{
private:
    std::unique_ptr<ArrayViewBase> _x;
    std::unique_ptr<ArrayViewBase> _y;
    std::unique_ptr<ArrayViewBase> _z;
    bool _y_is_2d;

    inline void _init_views(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
        std::size_t start, std::size_t stop)
    {
        _y_is_2d = (y.ndim() == 2) ? 1 : 0;
        this->_x = x.view(start, stop);
        this->_z = z.view(start, stop);
        if (_y_is_2d)
        {
            this->_y = y.view(start, stop);
        }
        else
        {
            this->_y = y.view();
        }
    }

public:
    explicit XYZView(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z, std::size_t start = 0,
        std::size_t stop = 0)
    {
        _init_views(x, y, z, start, stop);
    }

    explicit XYZView(
        const PyBuffer& x, const PyBuffer& y, const PyBuffer& z, double x_start, double x_stop)
    {
        std::size_t start_index = std::distance(
            x.data(), std::upper_bound(x.data(), x.data() + x.flat_size(), x_start));
        std::size_t stop_index
            = std::distance(x.data(), std::lower_bound(x.data(), x.data() + x.flat_size(), x_stop));
        _init_views(x, y, z, start_index, stop_index);
    }

    inline double x(std::size_t i) const { return (*_x)[{ i, 0 }]; }

    inline double y(std::size_t i, std::size_t j) const
    {
        if (_y_is_2d)
        {
            return (*_y)[{ i, j }];
        }
        return (*_y)[{ j, 0 }];
    }

    inline double z(std::size_t i, std::size_t j) const { return (*_z)[{ i, j }]; }

    inline std::size_t size() const { return std::size(*_x); }

    inline bool y_is_2d() const { return _y_is_2d; }

    inline std::pair<std::size_t, std::size_t> y_shape() const
    {
        if (_y_is_2d)
        {
            return { _y->size(0), _y->size(1) };
        }
        return { 1UL, _y->size(0) };
    }

    inline std::pair<std::size_t, std::size_t> z_shape() const
    {
        return { _z->size(0), _z->size(1) };
    }
};

namespace std
{
inline std::size_t size(const XYView& view)
{
    return view.size();
}

inline std::size_t size(const XYZView& view)
{
    return view.size();
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

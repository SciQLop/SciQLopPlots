/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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

#include "PythonInterface.hpp"

#include <assert.h>
#include <cmath>
#include <cpp_utils/warnings.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

// TODO add support numpy datetime64 (int64)
// Since speasy provides numpy datetime64 for time axis, we should add support for it.
// One option would be to convert it to double from the ctor
// and release the underlying buffer. This would require to check the unit of the datetime64 array.


struct XYView
{
private:
    std::unique_ptr<ArrayViewBase> _x;
    std::unique_ptr<ArrayViewBase> _y;

public:
    explicit XYView(const PyBuffer& x, const PyBuffer& y, std::size_t start = 0,
                    std::size_t stop = 0)
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

    explicit XYZView(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z, double x_start,
                     double x_stop)
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

inline double _nan_safe_min(const double a, const double b)
{
    if (std::isnan(a))
    {
        return b;
    }
    if (std::isnan(b))
    {
        return a;
    }
    return std::min(a, b);
}

inline double _nan_safe_max(const double a, const double b)
{
    if (std::isnan(a))
    {
        return b;
    }
    if (std::isnan(b))
    {
        return a;
    }
    return std::max(a, b);
}

inline std::pair<double, double> y_bounds(const XYZView& v, std::size_t row, std::size_t n_cols,
                                        double min = std::nan(""), double max = std::nan(""))
{
    {
        auto j = 0UL;
        auto val = v.y(row, j);
        while (std::isnan(val) && j < n_cols)
        {
            j++;
            val = v.y(row, j);
        }
        min = _nan_safe_min(val, min);
    }
    {
        auto j = n_cols - 1;
        auto val = v.y(row, j);
        while (std::isnan(val) && (j > 0))
        {
            j--;
            val = v.y(row, j);
        }
        max = _nan_safe_max(val, max);
    }

    return { min, max };
}

inline std::pair<double, double> y_bounds(const XYZView& v)
{
    auto shape = v.y_shape();
    if (!v.y_is_2d())
    {
        return y_bounds(v, 0, shape.second);
    }
    else
    {
        auto b = std::pair { std::nan(""), std::nan("") };
        for (auto i = 0UL; i < shape.first; i++)
        {
            b = y_bounds(v, i, shape.second, b.first, b.second);
        }
        return b;
    }
}

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

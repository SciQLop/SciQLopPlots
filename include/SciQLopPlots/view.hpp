/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2021, Plasma Physics Laboratory - CNRS
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

#include "axis_range.hpp"
#include "enums.hpp"
#include <hedley.h>

#include <array>
#include <cmath>

namespace SciQLopPlots::view
{


template <typename T>
struct single_coordinate
{
    T value;
    enums::Axis axis;
    single_coordinate(T value, enums::Axis axis) : value { value }, axis { axis } { }
};

using single_pixel_coordinate = single_coordinate<int>;
using single_data_coordinate = single_coordinate<double>;

namespace details
{
    template <typename callable_t, typename T, std::size_t ND, std::size_t... I>
    auto broadcast_impl(std::array<T, ND>& input, const callable_t& callable, std::index_sequence<I...>)
    {
        (callable(input[I]), ...);
    }

    template <typename callable_t, typename T, std::size_t ND>
    inline auto broadcast(std::array<T, ND>& input, const callable_t& callable)
    {
        broadcast_impl(input, callable, std::make_index_sequence<ND> {});
    }
}


template <typename T, std::size_t ND>
struct coordinates
{
    std::array<T, ND> _c;
    template <typename... U>
    coordinates(const U&&... values) : _c { { std::forward<const U>(values)... } }
    {
    }

    template <typename... U>
    coordinates(U&&... values) : _c { { std::forward<U>(values)... } }
    {
    }

    single_coordinate<T> component(const enums::Axis axis) const
    {
        return single_coordinate<T> { _c[static_cast<int>(axis)], axis };
    }

    T& component(const enums::Axis axis) { return _c[static_cast<int>(axis)]; }
};

template <typename tag, typename value_type, std::size_t ND>
struct ND_coordinates : coordinates<value_type, ND>
{
    template <typename... T>
    ND_coordinates(const T&&... values)
            : coordinates<value_type, ND> { std::forward<const T>(values)... }
    {
    }

    template <typename... T>
    ND_coordinates(T&&... values) : coordinates<value_type, ND> { static_cast<value_type>(values)... }
    {
    }

    ND_coordinates& operator+(value_type offset)
    {
        details::broadcast(this->_c, [offset](value_type& v)constexpr{return v+=offset;});
        return *this;
    }

    ND_coordinates operator+(value_type offset) const
    {
        auto copy = *this;
        details::broadcast(copy._c, [offset](value_type& v)constexpr{return v+=offset;});
        return copy;
    }

    ND_coordinates& operator-(value_type offset)
    {
        details::broadcast(this->_c, [offset](value_type& v)constexpr{return v-=offset;});
        return *this;
    }

    ND_coordinates operator-(value_type offset) const
    {
        auto copy = *this;
        details::broadcast(copy._c, [offset](value_type& v)constexpr{return v-=offset;});
        return copy;
    }

    ND_coordinates operator-() const
    {
        auto copy = *this;
        details::broadcast(copy._c, [](value_type& v)constexpr{return v=-v;});
        return copy;
    }
};

struct pixel_tag;
template <std::size_t ND>
struct pixel_coordinates : ND_coordinates<pixel_tag, int, ND>
{
    using ND_coordinates<pixel_tag, int, ND>::ND_coordinates;
    pixel_coordinates operator-() const
    {
        auto copy = *this;
        details::broadcast(copy._c, [](int& v)constexpr{return v=-v;});
        return copy;
    }
};
template <typename... T>
pixel_coordinates(const T&&... values) -> pixel_coordinates<sizeof...(T)>;
template <typename... T>
pixel_coordinates(T&&... values) -> pixel_coordinates<sizeof...(T)>;


struct data_tag;
template <std::size_t ND>
struct data_coordinates : ND_coordinates<data_tag, double, ND>
{
    using ND_coordinates<data_tag, double, ND>::ND_coordinates;

    data_coordinates operator-() const
    {
        auto copy = *this;
        details::broadcast(copy._c, [](double& v)constexpr{return v=-v;});
        return copy;
    }
};

template <typename... T>
data_coordinates(const T&&... values) -> data_coordinates<sizeof...(T)>;
template <typename... T>
data_coordinates(T&&... values) -> data_coordinates<sizeof...(T)>;


template <class Widget>
HEDLEY_NON_NULL(1)
inline auto to_data_coordinates(Widget* widget, const single_pixel_coordinate c)
{
    return single_data_coordinate { widget->map_pixels_to_data_coordinates(c.value, c.axis),
        c.axis };
}

template <class Widget>
HEDLEY_NON_NULL(1)
inline auto distance(const Widget* widget, const single_pixel_coordinate delta)
{
    return single_data_coordinate { to_data_coordinates(widget, delta).value
            - to_data_coordinates(widget, single_pixel_coordinate { 0, delta.axis }).value,
        delta.axis };
}

template <class Widget>
HEDLEY_NON_NULL(1)
inline auto distance(const Widget* widget, const pixel_coordinates<2> delta)
{
    return data_coordinates { distance(widget, delta.component(enums::Axis::x)).value,
        distance(widget, delta.component(enums::Axis::y)).value };
}


template <class Widget>
HEDLEY_NON_NULL(1)
inline auto set_range(Widget* widget, const axis::range& range, enums::Axis axis)
    -> decltype(widget->set_range(range, axis))
{
    widget->set_range(range, axis);
}

template <class Widget>
HEDLEY_NON_NULL(1)
inline auto set_range(Widget* widget, const axis::range& x_range, const axis::range& y_range)
    -> decltype(widget->set_range(x_range, y_range))
{
    widget->set_range(x_range, y_range);
}


template <class Widget>
HEDLEY_NON_NULL(1)
inline auto range(const Widget* widget, const enums::Axis axis) -> decltype(widget->range(axis))
{
    return widget->range(axis);
}


template <class Widget>
HEDLEY_NON_NULL(1)
inline void zoom(Widget* widget, const double factor, const enums::Axis axis)
{
    auto new_range = range(widget, axis) * factor;
    set_range(widget, new_range, axis);
}

template <class Widget>
HEDLEY_NON_NULL(1)
inline void zoom(Widget* widget, const double center, const double factor, const enums::Axis axis)
{
    const auto old_range = range(widget, axis);
    const auto offset = center * (1. - factor);
    axis::range new_range { factor * old_range.first + offset, factor * old_range.second + offset };
    set_range(widget, new_range, axis);
}

template <class Widget>
HEDLEY_NON_NULL(1)
inline void zoom(Widget* widget, const single_pixel_coordinate center, const double factor)
{
    zoom(widget, to_data_coordinates(widget, center).value, factor, center.axis);
}

template <class Widget>
HEDLEY_NON_NULL(1)
inline void move(Widget* widget, const double delta, const enums::Axis axis)
{
    const auto old_range = range(widget, axis);
    const auto new_range = old_range + (old_range.width() * delta);
    set_range(widget, new_range, axis);
}

template <class Widget>
HEDLEY_NON_NULL(1)
inline void move(Widget* widget, const single_data_coordinate& delta)
{
    set_range(widget, range(widget, delta.axis) + delta.value, delta.axis);
}


template <class Widget, std::size_t ND>
HEDLEY_NON_NULL(1)
inline void move(Widget* widget, const data_coordinates<ND>& delta)
{
    for (const auto axis : { enums::Axis::x, enums::Axis::y })
    {
        move(widget, delta.component(axis));
    }
}

template <class Widget, std::size_t ND>
HEDLEY_NON_NULL(1)
inline auto move(Widget* widget, const pixel_coordinates<ND>& delta)
    -> decltype(widget->map_pixels_to_data_coordinates(0, enums::Axis::x), void())
{
    move(widget, -distance(widget, delta));
}

template <class Widget, std::size_t ND>
HEDLEY_NON_NULL(1)
inline auto move(Widget* widget, const pixel_coordinates<ND>& delta)
    -> decltype(widget->move(delta), void())
{
    widget->move(delta);
}

}

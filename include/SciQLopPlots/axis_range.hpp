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

#include "enums.hpp"
#include <cmath>
#include <tuple>

namespace SciQLopPlots::axis
{

struct range
{
public:
    double first = 0.;
    double second = 0.;
    inline range() = default;
    inline ~range() = default;
    inline range(double lower, double upper) : first { lower }, second { upper } { }
    inline range(const range& other) : first { other.first }, second { other.second } { }
    inline range(range&& other) : first { other.first }, second { other.second } { }

    inline range& operator=(const range& other)
    {
        this->first = other.first;
        this->second = other.second;
        return *this;
    }

    inline double center() const noexcept { return (second + first) / 2.; }
    inline double width() const noexcept { return std::abs(second - first); }

    inline range& operator*=(double factor)
    {
        auto _center = center();
        auto newHalfWidth = width() * factor / 2;
        first = _center - newHalfWidth;
        second = _center + newHalfWidth;
        return *this;
    }

    inline range operator*(double factor) const
    {
        auto copy = *this;
        copy *= factor;
        return copy;
    }

    inline bool operator==(const range& other) const
    {
        return this->first == other.first && this->second == other.second;
    }

    inline bool operator!=(const range& other) const { return !(*this == other); }

    inline range& operator+=(double offset)
    {
        first += offset;
        second += offset;
        return *this;
    }

    inline range operator+(double offset) const
    {
        auto copy = *this;
        copy += offset;
        return copy;
    }

    inline range& operator-=(double offset)
    {
        first -= offset;
        second -= offset;
        return *this;
    }

    inline range operator-(double offset) const
    {
        auto copy = *this;
        copy -= offset;
        return copy;
    }
};

}

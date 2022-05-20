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

struct range : std::pair<double, double>
{
    range(double lower, double upper) : std::pair<double, double> { lower, upper } { }
    range(const range& other) : std::pair<double, double> { other.first, other.second } { }
    range(range&& other) : std::pair<double, double> { other.first, other.second } { }

    range& operator=(const range& other)
    {
        this->first = other.first;
        this->second = other.second;
        return *this;
    }

    double center() const noexcept { return (second + first) / 2.; }
    double width() const noexcept { return std::abs(second - first); }

    range& operator*=(double factor)
    {
        auto _center = center();
        auto newHalfWidth = width() * factor / 2;
        first = _center - newHalfWidth;
        second = _center + newHalfWidth;
        return *this;
    }

    range operator*(double factor) const
    {
        auto copy = *this;
        copy *=factor;
        return copy;
    }

    range& operator+=(double offset)
    {
        first += offset;
        second += offset;
        return *this;
    }

    range operator+(double offset) const
    {
        auto copy = *this;
        copy+=offset;
        return copy;
    }

    range& operator-=(double offset)
    {
        first -= offset;
        second -= offset;
        return *this;
    }

    range operator-(double offset)const
    {
        auto copy = *this;
        copy-=offset;
        return copy;
    }
};

}

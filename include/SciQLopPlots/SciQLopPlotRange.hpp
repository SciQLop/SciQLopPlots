/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
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

#include <QDateTime>
#include <QPair>
#include <utility>
#include <fmt/chrono.h>
#include <fmt/core.h>

struct SciQLopPlotRange:QPair<double, double>
{
protected:

public:
    SciQLopPlotRange() : QPair<double, double>{std::nan(""), std::nan("")} { }

    SciQLopPlotRange(double start, double stop)
            :  QPair<double, double>{std::min(start, stop), std::max(start, stop)}
    {
    }

    explicit SciQLopPlotRange(const QDateTime& start, const QDateTime& end)
            : QPair<double, double>{start.toSecsSinceEpoch(), end.toSecsSinceEpoch()}
    {
    }

    std::string __repr__() const
    {
        return fmt::format("SciQLopPlotRange({:%Y-%m-%d %H:%M:%S}, {:%Y-%m-%d %H:%M:%S})", start_date().toStdSysSeconds(), stop_date().toStdSysSeconds());
    }

    inline double start() const { return first; }

    inline double stop() const { return second; }

    inline QDateTime start_date() const { return QDateTime::fromSecsSinceEpoch(first); }
    inline QDateTime stop_date() const { return QDateTime::fromSecsSinceEpoch(second); }

    inline double size() const { return second - first; }

    inline double center() const { return (first + second) / 2; }

    inline bool is_empty() const { return first == second; }

    inline bool contains(double value) const { return first <= value && value <= second; }

    inline bool is_null() const { return std::isnan(first) || std::isnan(second); }

    inline bool is_valid() const { return !is_null() && !is_empty(); }

    inline bool contains(const SciQLopPlotRange& range) const
    {
        return first <= range.first && range.second <= second;
    }

    inline bool intersects(const SciQLopPlotRange& range) const
    {
        return first <= range.second && range.first <= second;
    }

    inline SciQLopPlotRange intersection_with(const SciQLopPlotRange& range) const
    {
        return SciQLopPlotRange(std::max(first, range.first), std::min(second, range.second));
    }

    inline SciQLopPlotRange union_with(const SciQLopPlotRange& range) const
    {
        return SciQLopPlotRange(std::min(first, range.first), std::max(second, range.second));
    }

    inline bool operator==(const SciQLopPlotRange& other) const
    {
        return first == other.first && second == other.second;
    }

    inline bool operator!=(const SciQLopPlotRange& other) const { return !(*this == other); }

    inline SciQLopPlotRange operator+(double value) const
    {
        return SciQLopPlotRange(first + value, second + value);
    }

    inline SciQLopPlotRange operator-(double value) const
    {
        return SciQLopPlotRange(first - value, second - value);
    }

    inline SciQLopPlotRange operator*(double value) const
    {
        auto sz = size();
        auto center = this->center();
        return SciQLopPlotRange(center - sz * value / 2, center + sz * value / 2);
    }

    inline SciQLopPlotRange operator/(double value) const
    {
        auto sz = size();
        auto center = this->center();
        return SciQLopPlotRange(center - sz / value / 2, center + sz / value / 2);
    }

    inline void sort()
    {
        if (first > second)
        {
            std::swap(first, second);
        }
    }

    SciQLopPlotRange sorted() const
    {
        auto copy = *this;
        copy.sort();
        return copy;
    }

    inline double operator[](int index) const { return index == 0 ? first : second; }

    inline double& operator[](int index) { return index == 0 ? first : second; }
};

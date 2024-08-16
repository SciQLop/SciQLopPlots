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
#include <utility>

struct SciQLopPlotRange
{
private:
    double m_start;
    double m_stop;

public:
    SciQLopPlotRange() : m_start(std::nan("")), m_stop(std::nan("")) { }
    SciQLopPlotRange(double start, double stop)
            : m_start(std::min(start, stop)), m_stop(std::max(stop, start))
    {
    }
    explicit SciQLopPlotRange(const QDateTime& start, const QDateTime& end)
            : m_start(start.toSecsSinceEpoch()), m_stop(end.toSecsSinceEpoch())
    {
    }

    inline double start() const { return m_start; }
    inline double stop() const { return m_stop; }
    inline double size() const { return m_stop - m_start; }
    inline double center() const { return (m_start + m_stop) / 2; }
    inline bool is_empty() const { return m_start == m_stop; }
    inline bool contains(double value) const { return m_start <= value && value <= m_stop; }

    inline bool contains(const SciQLopPlotRange& range) const
    {
        return m_start <= range.m_start && range.m_stop <= m_stop;
    }

    inline bool intersects(const SciQLopPlotRange& range) const
    {
        return m_start <= range.m_stop && range.m_start <= m_stop;
    }

    inline SciQLopPlotRange intersection_with(const SciQLopPlotRange& range) const
    {
        return SciQLopPlotRange(std::max(m_start, range.m_start), std::min(m_stop, range.m_stop));
    }

    inline SciQLopPlotRange union_with(const SciQLopPlotRange& range) const
    {
        return SciQLopPlotRange(std::min(m_start, range.m_start), std::max(m_stop, range.m_stop));
    }

    inline bool operator==(const SciQLopPlotRange& other) const
    {
        return m_start == other.m_start && m_stop == other.m_stop;
    }

    inline bool operator!=(const SciQLopPlotRange& other) const { return !(*this == other); }

    inline SciQLopPlotRange operator+(double value) const
    {
        return SciQLopPlotRange(m_start + value, m_stop + value);
    }

    inline SciQLopPlotRange operator-(double value) const
    {
        return SciQLopPlotRange(m_start - value, m_stop - value);
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
        if (m_start > m_stop)
        {
            std::swap(m_start, m_stop);
        }
    }

    SciQLopPlotRange sorted() const
    {
        auto copy = *this;
        copy.sort();
        return copy;
    }

    inline double operator[](int index) const { return index == 0 ? m_start : m_stop; }
    inline double& operator[](int index) { return index == 0 ? m_start : m_stop; }
};

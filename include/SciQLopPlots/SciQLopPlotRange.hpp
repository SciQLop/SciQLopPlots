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

#include <chrono>
#include <QDataStream>
#include <QDateTime>
#include <QTimeZone>
#include <QPair>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <utility>
#include <QRegularExpression>

inline int _capture_or_default(const QRegularExpressionMatch& match, const QString& name, int def)
{
    return match.captured(name).isEmpty() ? def : match.captured(name).toInt();
}

inline double str_date_to_double(const QString& str)
{
    static const QRegularExpression date_regex(R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})T? ?(?<hour>\d{2})?:?(?<minute>\d{2})?:?(?<second>\d{2})?\.?(?<millisecond>\d{3})?)");
    if(auto match = date_regex.match(str); match.hasMatch())
    {
        auto year = _capture_or_default(match, "year", 1973);
        auto month = _capture_or_default(match, "month", 1);
        auto day = _capture_or_default(match, "day", 1);
        auto hour = _capture_or_default(match, "hour", 0);
        auto minute = _capture_or_default(match, "minute", 0);
        auto second = _capture_or_default(match, "second", 0);
        auto millisecond = _capture_or_default(match, "millisecond", 0);
        QDateTime date(QDate(year, month, day), QTime(hour, minute, second, millisecond));
        date.setTimeZone(QTimeZone::utc());
        return date.toSecsSinceEpoch();
    }
    return std::nan("");
}

inline double date_to_double(const QDateTime& date)
{
    return static_cast<double>(date.toSecsSinceEpoch())+static_cast<double>(date.time().msec())/1000.0;
}

inline auto epoch_to_std_time(double epoch)
{
    return std::chrono::system_clock::from_time_t(epoch);
}


struct SciQLopPlotRange : QPair<double, double>
{
protected:
    bool _is_time_range = false;

public:
    SciQLopPlotRange() : QPair<double, double> { std::nan(""), std::nan("") } { }

    SciQLopPlotRange(const SciQLopPlotRange& other)
            : QPair<double, double> { other.first, other.second }
            , _is_time_range(other._is_time_range)
    {
    }

    explicit SciQLopPlotRange(const QDateTime& start, const QDateTime& end)
            : QPair<double, double> { date_to_double(start), date_to_double(end) }
            , _is_time_range(true)
    {
    }

    explicit SciQLopPlotRange(const QString& start, const QString& end)
            : QPair<double, double> { str_date_to_double(start), str_date_to_double(end) }
            , _is_time_range(true)
    {
    }

    SciQLopPlotRange(double start, double stop, bool is_time_range = false)
            : QPair<double, double> { std::min(start, stop), std::max(start, stop) }
            , _is_time_range(is_time_range)
    {
    }

    inline QDateTime start_date() const { return QDateTime::fromSecsSinceEpoch(first); }

    inline QDateTime stop_date() const { return QDateTime::fromSecsSinceEpoch(second); }

    inline SciQLopPlotRange& operator=(const SciQLopPlotRange& other)
    {
        first = other.first;
        second = other.second;
        return *this;
    }

    std::string __repr__() const
    {
        if (this->_is_time_range)
            return fmt::format("SciQLopPlotTimeRange({:%Y-%m-%d %H:%M:%S}, {:%Y-%m-%d %H:%M:%S})",
                               epoch_to_std_time(first), epoch_to_std_time(second));
        else
            return fmt::format("SciQLopPlotRange({}, {})", first, second);
    }

    inline double start() const { return first; }

    inline double stop() const { return second; }

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

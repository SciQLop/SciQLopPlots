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

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "AbstractResampler.hpp"

#include <qcustomplot.h>

template <bool Values>
struct QCPGraphDataVectorIterator
{
    using value_type = double;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using pointer = double*;
    using reference = double&;
    using const_pointer = const double*;
    using const_reference = const double&;

    QCPGraphDataVectorIterator(QVector<QCPGraphData>::const_iterator it) : _it(it) { }

    QCPGraphDataVectorIterator& operator++()
    {
        ++_it;
        return *this;
    }

    QCPGraphDataVectorIterator operator++(int)
    {
        auto tmp = *this;
        ++_it;
        return tmp;
    }

    QCPGraphDataVectorIterator& operator--()
    {
        --_it;
        return *this;
    }

    QCPGraphDataVectorIterator operator--(int)
    {
        auto tmp = *this;
        --_it;
        return tmp;
    }

    QCPGraphDataVectorIterator& operator+=(difference_type n)
    {
        _it += n;
        return *this;
    }

    QCPGraphDataVectorIterator operator+(difference_type n) const
    {
        return QCPGraphDataVectorIterator(_it + n);
    }

    QCPGraphDataVectorIterator& operator-=(difference_type n)
    {
        _it -= n;
        return *this;
    }

    QCPGraphDataVectorIterator operator-(difference_type n) const
    {
        return QCPGraphDataVectorIterator(_it - n);
    }

    difference_type operator-(const QCPGraphDataVectorIterator& other) const
    {
        return _it - other._it;
    }

    const_reference operator*() const { return Values ? _it->value : _it->key; }

    const_reference operator[](difference_type n) const
    {
        return Values ? (_it + n)->value : (_it + n)->key;
    }

    bool operator==(const QCPGraphDataVectorIterator& other) const { return _it == other._it; }

    bool operator!=(const QCPGraphDataVectorIterator& other) const { return _it != other._it; }

    bool operator<(const QCPGraphDataVectorIterator& other) const { return _it < other._it; }

    bool operator>(const QCPGraphDataVectorIterator& other) const { return _it > other._it; }

    bool operator<=(const QCPGraphDataVectorIterator& other) const { return _it <= other._it; }

    bool operator>=(const QCPGraphDataVectorIterator& other) const { return _it >= other._it; }

    const_pointer operator->() const { return Values ? &_it->value : &_it->key; }

    const_reference value() const { return _it->value; }

    const_reference key() const { return _it->key; }

private:
    QVector<QCPGraphData>::const_iterator _it;
};

struct ResamplingContextLevelView
{
    QCPGraphDataVectorIterator<false> keys_begin;
    QCPGraphDataVectorIterator<false> keys_end;
    QCPGraphDataVectorIterator<true> values_begin;
    QCPGraphDataVectorIterator<true> values_end;

    inline bool contains(const double xmin, const double xmax) const
    {
        return keys_begin != keys_end && keys_begin.key() <= xmin && keys_end.key() >= xmax;
    }

    inline std::size_t size() const { return std::distance(keys_begin, keys_end); }
};

static inline QVector<QCPGraphData> resample(const auto& x_begin, const auto& y_begin,
                                             std::size_t source_size, std::size_t dest_size)
{
    PROFILE_HERE_N("QVector<QCPGraphData> resample");
    if (dest_size & 1)
        dest_size++;
    const auto dx = 2. * (*(x_begin + source_size - 1) - *x_begin) / static_cast<double>(dest_size);
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QVector<QCPGraphData> data(dest_size);
    {
        auto key = *x_begin;
        std::size_t dest_index = 0UL;
        auto next_x = key + dx;
        data[0].key = key;
        data[0].value = nan;
        data[1].key = key + dx * 0.5;
        data[1].value = nan;
        for (auto [x_it, y_it, idx] = std::tuple { x_begin, y_begin, 0UL }; idx < source_size;
             x_it++, y_it++, idx++)
        {
            auto x = *x_it;
            auto y = *y_it;
            while (x > next_x && dest_index < dest_size - 4)
            {
                dest_index += 2;
                data[dest_index].key = next_x;
                data[dest_index].value = nan;
                data[dest_index + 1].key = next_x + dx * 0.5;
                data[dest_index + 1].value = nan;
                next_x += dx * 2.;
            }

            if (!std::isnan(y)) [[likely]]
            {
                // NaN comparison is always false
                // Either value is NaN or y is smaller than value
                if (!(data[dest_index].value < y))
                    data[dest_index].value = y;
                // Either value is NaN or y is greater than value
                if (!(data[dest_index + 1].value > y))
                    data[dest_index + 1].value = y;
            }
        }
        if (dest_index < dest_size - 1)
        {
            data.resize(dest_index + 2);
        }
    }
    return data;
}

struct ResamplingContextLevel
{
    std::optional<QVector<QCPGraphData>> data;
    double source_min_x;
    double source_max_x;

    inline void reset()
    {
        data.reset();
        source_min_x = std::numeric_limits<double>::quiet_NaN();
        source_max_x = std::numeric_limits<double>::quiet_NaN();
    }

    inline bool contains(const double xmin, const double xmax) const
    {
        return data.has_value() && !data->isEmpty()
            && (source_min_x <= xmin && source_max_x >= xmax);
    }

    inline void update(const XYView& view, std::size_t column_index)
    {
        PROFILE_HERE_N("ResamplingContextLevel update");
        data = resample(view.x_row_iterator(0), view.y_row_iterator(0, column_index),
                        std::size(view), 1'000'000);
        source_min_x = view.x(0);
        source_max_x = view.x(view.size() - 1);
    }

    inline const auto keys_begin() const
    {
        return QCPGraphDataVectorIterator<false>(data->cbegin());
    }

    inline const auto keys_end() const { return QCPGraphDataVectorIterator<false>(data->cend()); }

    inline const auto values_begin() const
    {
        return QCPGraphDataVectorIterator<true>(data->cbegin());
    }

    inline const auto values_end() const { return QCPGraphDataVectorIterator<true>(data->cend()); }

    inline ResamplingContextLevelView view(const double xmin, const double xmax) const
    {
        if (!contains(xmin, xmax))
            return ResamplingContextLevelView { keys_begin(), keys_end(), values_begin(),
                                                values_end() };

        auto begin_it
            = std::lower_bound(data->cbegin(), data->cend(), xmin,
                               [](const QCPGraphData& data, double x) { return data.key < x; });
        auto end_it
            = std::upper_bound(data->cbegin(), data->cend(), xmax,
                               [](double x, const QCPGraphData& data) { return x < data.key; });

        return ResamplingContextLevelView { QCPGraphDataVectorIterator<false>(begin_it),
                                            QCPGraphDataVectorIterator<false>(end_it),
                                            QCPGraphDataVectorIterator<true>(begin_it),
                                            QCPGraphDataVectorIterator<true>(end_it) };
    }
};

static inline QVector<QCPGraphData> resample(const XYView& view, std::size_t column_index,
                                             std::size_t dest_size, ResamplingContextLevel& context)
{
    PROFILE_HERE_N("QVector<QCPGraphData> resample");
    const auto view_size = std::size(view);

    // If the view is more than 10 million points, we will use a multilayer resampling strategy
    // this means that we will create an intermediate vector with 1 million points, on which we will
    // resample to the final destination size. This is really useful when displaying large static
    // datasets and doing a lot of zooming and panning.
    if (view_size >= 10'000'000)
    {
        if (!context.contains(view.x(0), view.x(view_size - 1)))
        {
            // We need to resample the data to 1 million points first
            // This is a lot faster than resampling directly to the destination size
            // and allows us to reuse the data for later transformations.
            context.update(view, column_index);
        }
        // Now we can resample the data to the final destination size
        const auto context_view = context.view(view.x(0), view.x(view_size - 1));
        return resample(context_view.keys_begin, context_view.values_begin, context_view.size(),
                        dest_size);
    }
    return resample(view.x_row_iterator(0), view.y_row_iterator(0, column_index), view_size,
                    dest_size);
}

struct LineGraphResampler : public AbstractResampler1d
{
    Q_OBJECT

    QList<ResamplingContextLevel> _resampling_context;

    void _resample_impl(const ResamplerData1d& data, const ResamplerPlotInfo& plot_info) override;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(QList<QVector<QCPGraphData>> data);
#endif

    LineGraphResampler(SciQLopPlottableInterface* parent, std::size_t line_cnt);
};

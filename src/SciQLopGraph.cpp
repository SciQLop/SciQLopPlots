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
#include "SciQLopPlots/SciQLopGraph.hpp"


template <std::size_t dest_size>
static inline QVector<QCPGraphData> resample(
    const double* x, const double* y, std::size_t x_size, const int y_incr = 1)
{
    static_assert(dest_size % 2 == 0);
    const auto x_0 = x[0];
    const auto x_1 = x[x_size - 1];
    auto dx = 2. * (x_1 - x_0) / dest_size;
    QVector<QCPGraphData> data(dest_size);
    const double* current_x = x;
    const double* current_y_it = y;
    for (auto bucket = 0UL; bucket < dest_size / 2; bucket++)
    {
        {
            const auto bucket_start_x = *current_x;
            double bucket_max_x = std::min(bucket_start_x + dx, x_1);
            double max_value = std::nan("");
            double min_value = std::nan("");
            while (*current_x < bucket_max_x)
            {
                max_value = std::fmax(max_value, *current_y_it);
                min_value = std::fmin(min_value, *current_y_it);
                current_y_it += y_incr;
                current_x++;
            }
            data[2 * bucket] = QCPGraphData { bucket_start_x, min_value };
            data[(2 * bucket) + 1] = QCPGraphData { bucket_start_x + dx / 2., max_value };
        }
    }
    return data;
}

static inline QVector<QCPGraphData> copy_data(
    const double* x, const double* y, std::size_t x_size, const int y_incr = 1)
{
    QVector<QCPGraphData> data(x_size);
    const double* current_y_it = y;
    for (auto i = 0UL; i < x_size; i++)
    {
        data[i] = QCPGraphData { x[i], *current_y_it };
        current_y_it += y_incr;
    }
    return data;
}

void SciQLopGraph::_range_changed(const QCPRange& newRange, const QCPRange& oldRange)
{

    if (!(newRange.contains(_data_x_range.lower) && newRange.contains(_data_x_range.upper)
            && oldRange.contains(_data_x_range.lower) && oldRange.contains(_data_x_range.upper)))
        this->_resample(newRange);

    if (!(_data_x_range.contains(newRange.lower) && _data_x_range.contains(newRange.upper)))
        emit this->range_changed(newRange, true);
    else
        emit this->range_changed(newRange, false);
}

void SciQLopGraph::_resample(const QCPRange& newRange)
{
    QMutexLocker locker(&_data_swap_mutex);
    if (_x.data() != nullptr && _x.flat_size() > 0)
    {
        const auto start_x
            = std::upper_bound(_x.data(), _x.data() + _x.flat_size(), newRange.lower);
        const auto end_x = std::lower_bound(_x.data(), _x.data() + _x.flat_size(), newRange.upper);
        const auto x_window_size = end_x - start_x;
        if (x_window_size > 0)
        {
            const auto line_cnt = static_cast<std::size_t>(std::size(_graphs));
            const auto y_incr = (_dataOrder == DataOrder::xFirst) ? 1UL : line_cnt;
            for (auto line_index = 0UL; line_index < line_cnt; line_index++)
            {
                const auto start_y = _y.data() + y_incr * (start_x - _x.data())
                    + (line_index * ((_dataOrder == DataOrder::xFirst) ? _x.flat_size() : 1));
                if (x_window_size > 10000)
                {
                    emit this->_setGraphDataSig(
                        line_index, resample<10000>(start_x, start_y, x_window_size, y_incr));
                }
                else
                {
                    emit this->_setGraphDataSig(
                        line_index, copy_data(start_x, start_y, x_window_size, y_incr));
                }
            }
        }
    }
}

void SciQLopGraph::_setGraphData(std::size_t index, QVector<QCPGraphData> data)
{
    _graphs[index]->data()->set(data, true);
}

SciQLopGraph::SciQLopGraph(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QStringList labels, DataOrder dataOrder)
        : QObject(parent), _keyAxis { keyAxis }, _valueAxis { valueAxis }, _dataOrder { dataOrder }
{
    this->_create_graphs(labels);
    connect(keyAxis, QOverload<const QCPRange&, const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        QOverload<const QCPRange&, const QCPRange&>::of(&SciQLopGraph::_range_changed));
    connect(this, &SciQLopGraph::_setGraphDataSig, this, &SciQLopGraph::_setGraphData,
        Qt::QueuedConnection);
}

SciQLopGraph::~SciQLopGraph()
{
    for (auto graph : _graphs)
    {
        this->_plot()->removeGraph(graph);
    }
}

void SciQLopGraph::setData(NpArray_view&& x, NpArray_view&& y, bool ignoreCurrentRange)
{
    {
        QMutexLocker locker(&_data_swap_mutex);

        _x = std::move(x);
        _y = std::move(y);
    }
    const auto len = _x.flat_size();
    if (len > 0)
    {
        _data_x_range.lower = _x.data()[0];
        _data_x_range.upper = _x.data()[len - 1];
    }
    else
    {
        _data_x_range.lower = std::nan("");
        _data_x_range.upper = std::nan("");
    }
    this->_resample(_data_x_range);
    this->_plot()->replot(QCustomPlot::rpQueuedReplot);
}

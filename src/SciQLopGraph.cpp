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
#include <iostream>

template <std::size_t dest_size>
static inline QVector<QCPGraphData> resample(
    const double* x, const double* y, std::size_t x_size, const int y_incr = 1)
{
    const auto x_0 = x[0];
    const auto x_1 = x[x_size - 1];
    auto dx = (x_1 - x_0) / dest_size;
    QVector<QCPGraphData> data(dest_size);
    const double* current_x = x;
    const double* current_y_it = y;
    std::cout << "resample " << dest_size << " " << x_size << " " << y_incr << " " << dx
              << std::endl;
    for (auto bucket = 0UL; bucket < dest_size;)
    {
        {
            double bucket_max_x = std::min(x_0 + (bucket + 1) * dx, x_1);
            double max_value = std::nan("");
            while (*current_x < bucket_max_x)
            {
                max_value = std::fmax(max_value, *current_y_it);
                current_y_it += y_incr;
                current_x++;
            }
            data[bucket] = QCPGraphData { x_0 + bucket * dx, max_value };
        }
        bucket++;
        {
            double bucket_max_x = std::min(x_0 + (bucket + 1) * dx, x_1);
            double min_value = std::nan("");
            while (*current_x < bucket_max_x)
            {
                min_value = std::fmin(min_value, *current_y_it);
                current_y_it += y_incr;
                current_x++;
            }
            data[bucket] = QCPGraphData { x_0 + bucket * 1.5 * dx, min_value };
        }
        bucket++;
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

void SciQLopGraph::_resample(const QCPRange& newRange)
{
    std::cout << newRange.lower << " " << newRange.upper << std::endl;
    if (_x.flat_size() > 0 && _x.data() != nullptr)
    {
        const auto start_x
            = [start = _x.data(), end = _x.data() + _x.flat_size(), bound = newRange.lower]()
        {
            auto pos = start;
            while (*pos < bound && pos != end)
            {
                pos++;
            }
            return pos == start ? pos : pos - 1;
        }();
        const auto end_x
            = [start = _x.data(), end = _x.data() + _x.flat_size(), bound = newRange.upper]()
        {
            auto pos = end - 1;
            while (*pos > bound && pos != start)
            {
                pos--;
            }
            return pos;
        }();
        const auto x_window_size = end_x - start_x;
        std::cout << x_window_size << std::endl;
        const auto line_cnt = static_cast<std::size_t>(std::size(_graphs));
        const auto y_incr = (_dataOrder == DataOrder::xFirst) ? 1UL : line_cnt;
        for (auto line_index = 0UL; line_index < line_cnt; line_index++)
        {
            const auto start_y = _y.data() + y_incr * (start_x - _x.data())
                + (line_index * ((_dataOrder == DataOrder::xFirst) ? _x.flat_size() : 1));
            if (x_window_size > 10000)
            {
                _graphs[line_index]->data()->set(
                    resample<10000>(start_x, start_y, x_window_size, y_incr), true);
            }
            else
            {
                _graphs[line_index]->data()->set(
                    copy_data(start_x, start_y, x_window_size, y_incr), true);
            }
        }
    }
}

SciQLopGraph::SciQLopGraph(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QStringList labels, DataOrder dataOrder)
        : QObject(parent), _keyAxis { keyAxis }, _valueAxis { valueAxis }, _dataOrder { dataOrder }
{
    this->_create_graphs(labels);
    connect(keyAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        &SciQLopGraph::_resample);
}

SciQLopGraph::~SciQLopGraph() { }

void SciQLopGraph::setData(NpArray_view&& x, NpArray_view&& y)
{
    _x = std::move(x);
    _y = std::move(y);
    this->_resample(_plot()->xAxis->range());
}

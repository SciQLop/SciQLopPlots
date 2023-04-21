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

#include "SciQLopGraph.hpp"
#include "numpy_wrappers.hpp"
#include <QMutex>
#include <qcustomplot.h>


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


struct GraphResampler : public QObject
{
    Q_OBJECT
    QMutex _mutex;
    NpArray_view _x;
    NpArray_view _y;
    SciQLopGraph::DataOrder _dataOrder;
    QCPRange _data_x_range;
    std::size_t _line_cnt;

    Q_SIGNAL void _resample_sig(const QCPRange& newRange);
    inline void _resample_slot(const QCPRange& newRange)
    {
        QMutexLocker locker(&_mutex);
        if (_x.data() != nullptr && _x.flat_size() > 0)
        {
            const auto start_x
                = std::upper_bound(_x.data(), _x.data() + _x.flat_size(), newRange.lower);
            const auto end_x
                = std::lower_bound(_x.data(), _x.data() + _x.flat_size(), newRange.upper);
            const auto x_window_size = end_x - start_x;
            if (x_window_size > 0)
            {
                const auto y_incr
                    = (_dataOrder == SciQLopGraph::DataOrder::xFirst) ? 1UL : _line_cnt;
                for (auto line_index = 0UL; line_index < _line_cnt; line_index++)
                {
                    const auto start_y = _y.data() + y_incr * (start_x - _x.data())
                        + (line_index
                            * ((_dataOrder == SciQLopGraph::DataOrder::xFirst) ? _x.flat_size()
                                                                               : 1));
                    if (x_window_size > 10000)
                    {
                        emit this->setGraphData(
                            line_index, ::resample<10000>(start_x, start_y, x_window_size, y_incr));
                    }
                    else
                    {
                        emit this->setGraphData(
                            line_index, copy_data(start_x, start_y, x_window_size, y_incr));
                    }
                }
            }
            emit this->refreshPlot();
        }
    }

public:
    Q_SIGNAL void setGraphData(std::size_t index, QVector<QCPGraphData> data);
    Q_SIGNAL void refreshPlot();

    GraphResampler(SciQLopGraph::DataOrder dataOrder, std::size_t line_cnt)
            : _dataOrder { dataOrder }, _line_cnt { line_cnt }
    {

        connect(this, &GraphResampler::_resample_sig, this, &GraphResampler::_resample_slot,
            Qt::QueuedConnection);
    }

    inline void resample(const QCPRange& newRange) { emit this->_resample_sig(newRange); }

    inline QCPRange x_range()
    {
        QMutexLocker locker(&_mutex);
        return this->_data_x_range;
    }

    inline void set_line_count(std::size_t line_cnt)
    {
        QMutexLocker locker(&_mutex);
        this->_line_cnt = line_cnt;
    }

    inline void setData(NpArray_view&& x, NpArray_view&& y)
    {
        {
            QMutexLocker locker(&_mutex);

            _x = std::move(x);
            _y = std::move(y);

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
        }
        this->resample(_data_x_range);
    }
};

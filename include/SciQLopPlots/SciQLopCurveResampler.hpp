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

#include "BufferProtocol.hpp"
#include "SciQLopCurve.hpp"
#include "SciQLopGraph.hpp"
#include <QMutex>
#include <qcustomplot.h>


static inline QVector<QCPCurveData> copy_data(
    const double* x, const double* y, std::size_t x_size, const int y_incr = 1)
{
    QVector<QCPCurveData> data(x_size);
    const double* current_y_it = y;
    for (auto i = 0UL; i < x_size; i++)
    {
        data[i] = QCPCurveData { static_cast<double>(i), x[i], *current_y_it };
        current_y_it += y_incr;
    }
    return data;
}


struct CurveResampler : public QObject
{
    Q_OBJECT
    QMutex _data_mutex;
    QMutex _range_mutex;
    Array_view _x;
    Array_view _y;
    SciQLopGraph::DataOrder _dataOrder;
    QCPRange _data_x_range;
    std::size_t _line_cnt;

    Q_SIGNAL void _resample_sig(const QCPRange newRange);
    inline void _resample_slot(const QCPRange newRange)
    {
        QMutexLocker locker(&_data_mutex);
        if (_x.data() != nullptr && _x.flat_size() > 0)
        {


            const auto y_incr = (_dataOrder == SciQLopGraph::DataOrder::xFirst) ? 1UL : _line_cnt;
            for (auto line_index = 0UL; line_index < _line_cnt; line_index++)
            {
                const auto count = std::size(_x);
                const auto start_y = _y.data()
                    + (line_index
                        * ((_dataOrder == SciQLopGraph::DataOrder::xFirst) ? _x.flat_size() : 1));
                emit this->setGraphData(line_index, copy_data(_x.data(), start_y, count, y_incr));
            }
            _x.release();
            _y.release();
        }
        emit this->refreshPlot();
    }

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(std::size_t index, QVector<QCPCurveData> data);
    Q_SIGNAL void refreshPlot();
#endif

    CurveResampler(SciQLopGraph::DataOrder dataOrder, std::size_t line_cnt)
            : _dataOrder { dataOrder }, _line_cnt { line_cnt }
    {

        connect(this, &CurveResampler::_resample_sig, this, &CurveResampler::_resample_slot,
            Qt::QueuedConnection);
    }

    inline void resample(const QCPRange newRange) { emit this->_resample_sig(newRange); }

    inline QCPRange x_range()
    {
        QMutexLocker locker(&_range_mutex);
        auto rng = this->_data_x_range;
        return rng;
    }

    inline void set_line_count(std::size_t line_cnt)
    {
        QMutexLocker locker(&_data_mutex);
        this->_line_cnt = line_cnt;
    }

    inline void setData(Array_view&& x, Array_view&& y)
    {
        {
            QMutexLocker locker(&_data_mutex);

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
            this->resample(_data_x_range);
        }
    }
};

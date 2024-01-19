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

#include "BufferProtocol.hpp"
#include <QMutex>
#include <cpp_utils/containers/algorithms.hpp>
#include <qcustomplot.h>


struct ColormapResampler : public QObject
{
    Q_OBJECT
    QMutex _mutex;
    QMutex _range_mutex;
    Array_view _x;
    Array_view _y;
    Array_view _z;
    QCPRange _data_x_range;
    std::size_t _line_cnt;
    QCPAxis::ScaleType _scale_type;
    QCPColorMapData* _last_data_ptr = nullptr;
    std::size_t _max_y_size = 1000;


    std::vector<double> _optimal_y_scale(const Array_view& x, const Array_view& y, QCPAxis::ScaleType scale_type);

    QCPColorMapData* _setDataLinear(const Array_view& x, const Array_view& y,const  Array_view& z);

    QCPColorMapData* _setDataLog(const Array_view& x, const Array_view& y, const Array_view& z);

    Q_SIGNAL void _resample_sig(const QCPRange& newRange);
    inline void _resample_slot(const QCPRange& newRange)
    {
        {
            QMutexLocker lock { &_range_mutex };
            if (this->_data_x_range != newRange)
                return;
        }
        _mutex.lock();
        auto x = std::move(_x);
        auto y = std::move(_y);
        auto z = std::move(_z);
        _mutex.unlock();
        if (std::size(x) && std::size(y) && std::size(z))
        {
            if (this->_scale_type == QCPAxis::stLinear)
            {
                emit this->refreshPlot(this->_setDataLinear(x,y,z));
            }
            else
            {
                emit this->refreshPlot(this->_setDataLog(x, y, z));
            }
        }
        else
        {
            emit this->refreshPlot(new QCPColorMapData(0, 0, { 0., 0. }, { 0., 0. }));
        }
    }

public:
    Q_SIGNAL void setGraphData(std::size_t index, QVector<QCPGraphData> data);
    Q_SIGNAL void refreshPlot(QCPColorMapData* data);

    ColormapResampler(QCPAxis::ScaleType scale_type) : _scale_type { scale_type }
    {
        connect(this, &ColormapResampler::_resample_sig, this, &ColormapResampler::_resample_slot,
            Qt::QueuedConnection);
    }

    inline void resample(const QCPRange& newRange) { emit this->_resample_sig(newRange); }

    inline QCPRange x_range()
    {
        QMutexLocker locker(&_mutex);
        return this->_data_x_range;
    }

    inline void setData(
        Array_view&& x, Array_view&& y, Array_view&& z, QCPAxis::ScaleType scale_type)
    {
        const auto len = x.flat_size();
        {
            QMutexLocker range_lock { &_range_mutex };
            if (len > 0)
            {
                _data_x_range.lower = x.data()[0];
                _data_x_range.upper = x.data()[len - 1];
            }
            else
            {
                _data_x_range.lower = std::nan("");
                _data_x_range.upper = std::nan("");
            }
        }
        {
            QMutexLocker data_locker(&_mutex);

            _x = std::move(x);
            _y = std::move(y);
            _z = std::move(z);

            _scale_type = scale_type;
        }
        this->resample(_data_x_range);
    }
};

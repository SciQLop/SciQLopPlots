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


#include <QMutex>
#include <cpp_utils/containers/algorithms.hpp>
#include <qcustomplot.h>


struct ColormapResampler : public QObject
{
    Q_OBJECT
    QMutex _mutex;
    QMutex _range_mutex;
    PyBuffer _x;
    PyBuffer _y;
    PyBuffer _z;
    QCPRange _data_x_range;
    std::size_t _line_cnt;
    QCPAxis::ScaleType _scale_type;
    QCPColorMapData* _last_data_ptr = nullptr;
    std::size_t _max_y_size = 1000;


    std::vector<double> _optimal_y_scale(
        const PyBuffer& x, const PyBuffer& y, QCPAxis::ScaleType scale_type);

    QCPColorMapData* _setDataLinear(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z);

    QCPColorMapData* _setDataLog(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z);

#ifndef BINDINGS_H
    Q_SIGNAL void _resample_sig(const QCPRange& newRange);
#endif // !BINDINGS_H
    void _resample_slot(const QCPRange& newRange);

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(std::size_t index, QVector<QCPGraphData> data);
    Q_SIGNAL void refreshPlot(QCPColorMapData* data);
#endif // !BINDINGS_H

    ColormapResampler(QCPAxis::ScaleType scale_type);

    void resample(const QCPRange& newRange);

    inline QCPRange x_range()
    {
        QMutexLocker locker(&_mutex);
        return this->_data_x_range;
    }

    inline void setData(
        PyBuffer&& x, PyBuffer&& y, PyBuffer&& z, QCPAxis::ScaleType scale_type)
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

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

#include "numpy_wrappers.hpp"
#include <QMutex>
#include <cpp_utils/containers/algorithms.hpp>
#include <qcustomplot.h>


struct ColormapResampler : public QObject
{
    Q_OBJECT
    QMutex _mutex;
    QMutex _range_mutex;
    NpArray_view _x;
    NpArray_view _y;
    NpArray_view _z;
    QCPRange _data_x_range;
    std::size_t _line_cnt;
    QCPAxis::ScaleType _scale_type;
    QCPColorMapData* _last_data_ptr = nullptr;


    inline QCPColorMapData* _setDataLinear(const NpArray_view& x, const NpArray_view& y,const  NpArray_view& z)
    {

            using namespace cpp_utils;
            auto data = new QCPColorMapData(std::size(x), std::size(y),
                { *containers::min(x), *containers::max(x) },
                { *containers::min(y), *containers::max(y) });
            auto it = std::cbegin(z);
            for (const auto i : x)
            {
                for (const auto j : y)
                {
                    if (it != std::cend(z))
                    {
                        data->setData(i, j, *it);
                        it++;
                    }
                }
            }
            return data;
    }

    inline QCPColorMapData* _setDataLog(const NpArray_view& x, const NpArray_view& y, const NpArray_view& z)
    {
        using namespace cpp_utils;
        auto data = new QCPColorMapData(std::size(x), std::size(y),
            { *containers::min(x), *containers::max(x) },
            { *containers::min(y), *containers::max(y) });
        auto it = std::cbegin(z);
        for (auto i = 0UL; i < std::size(x); i++)
        {
            for (auto j = 0UL; j < std::size(y); j++)
            {
                if (it != std::cend(z))
                {
                    data->setCell(i, j, *it);
                    it++;
                }
            }
        }
        return data;
    }

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
                emit this->refreshPlot(this->_setDataLog(std::move(x), std::move(y), std::move(z)));
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
        NpArray_view&& x, NpArray_view&& y, NpArray_view&& z, QCPAxis::ScaleType scale_type)
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

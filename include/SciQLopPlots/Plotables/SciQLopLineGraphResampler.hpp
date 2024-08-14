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


#include "SciQLopPlots/enums.hpp"

#include <QMutex>
#include <qcustomplot.h>


template <std::size_t dest_size>
static inline QVector<QCPGraphData> resample(
    const double* x, const ArrayViewBase& y, std::size_t column_index, std::size_t x_size)
{
    static_assert(dest_size % 2 == 0);
    const auto x_0 = x[0];
    const auto x_1 = x[x_size - 1];
    auto dx = 2. * (x_1 - x_0) / dest_size;
    QVector<QCPGraphData> data(dest_size);
    const double* current_x = x;
    std::size_t row_index = 0;
    std::size_t data_index = 0;
    for (auto bucket = 0UL; bucket < dest_size / 2; bucket++)
    {
        {
            const auto bucket_start_x = *current_x;
            double bucket_max_x = std::min(bucket_start_x + dx, x_1);
            double max_value = std::nan("");
            double min_value = std::nan("");
            while (*current_x < bucket_max_x)
            {
                auto v = y[{ row_index++, column_index }];
                max_value = std::fmax(max_value, v);
                min_value = std::fmin(min_value, v);
                current_x++;
            }
            data[data_index++] = QCPGraphData { bucket_start_x, min_value };
            data[data_index++] = QCPGraphData { bucket_start_x + dx / 2., max_value };
        }
    }
    return data;
}

static inline QVector<QCPGraphData> copy_data(
    const double* x, const ArrayViewBase& y, std::size_t column_index, std::size_t x_size)
{
    QVector<QCPGraphData> data(x_size);
    for (auto i = 0UL; i < x_size; i++)
    {
        data[i] = QCPGraphData { x[i], y[{ i, column_index }] };
    }
    return data;
}

struct ResamplerData
{
    PyBuffer x;
    PyBuffer y;
    QCPRange _x_range;
    QCPRange _plot_range;
    bool new_data = true;
};

struct AbstractResampler1d : public QObject
{
protected:
    Q_OBJECT
    QRecursiveMutex _data_mutex;
    QRecursiveMutex _next_data_mutex;
    ResamplerData _data;
    ResamplerData _next_data;
    std::size_t _line_cnt;
#ifndef BINDINGS_H
    Q_SIGNAL void _resample_sig();
#endif
    void _resample_slot();

    virtual void _resample(
        const PyBuffer& x, const PyBuffer& y, const QCPRange new_range, bool new_data)
        = 0;

public:
    AbstractResampler1d(std::size_t line_cnt);

    void resample(const QCPRange new_range);

    inline QCPRange x_range()
    {
        QMutexLocker locker(&_data_mutex);
        auto rng = this->_data._x_range;
        return rng;
    }

    inline void set_line_count(std::size_t line_cnt)
    {
        QMutexLocker locker(&_data_mutex);
        this->_line_cnt = line_cnt;
    }

    inline void setData(PyBuffer&& x, PyBuffer&& y)
    {
        {
            QCPRange _data_x_range;
            const auto len = x.flat_size();
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
            QMutexLocker locker(&_next_data_mutex);
            _next_data
                = ResamplerData { std::move(x), std::move(y), _data_x_range, _data_x_range, true };
            this->resample(_data_x_range);
        }
    }

    inline std::size_t line_count() const { return _line_cnt; }

    QList<PyBuffer> get_data()
    {
        QMutexLocker locker(&_data_mutex);
        return { _data.x, _data.y };
    }

#ifndef BINDINGS_H
    Q_SIGNAL void refreshPlot();
#endif
};

struct LineGraphResampler : public AbstractResampler1d
{
    Q_OBJECT

    void _resample(
        const PyBuffer& x, const PyBuffer& y, const QCPRange new_range, bool new_data) override;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(std::size_t index, QVector<QCPGraphData> data);
#endif

    LineGraphResampler(std::size_t line_cnt);
};

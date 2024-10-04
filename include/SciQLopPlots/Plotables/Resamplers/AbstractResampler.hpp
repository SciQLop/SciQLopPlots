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
#include <QMutex>
#include <QObject>
#include <QRecursiveMutex>
#include <QVector>
#include <SciQLopPlots/Python/PythonInterface.hpp>
#include <qcustomplot.h>

static inline QVector<QCPGraphData> copy_data(const XYView& view, std::size_t column_index)
{
    QVector<QCPGraphData> data(std::size(view));
    for (decltype(std::size(data)) i = 0UL; i < std::size(data); i++)
    {
        data[i] = QCPGraphData { view.x(i), view.y(i, column_index) };
    }
    return data;
}

struct ResamplerData1d
{
    PyBuffer x;
    PyBuffer y;
    QCPRange _x_range;
    QCPRange _plot_range;
    bool new_data = true;
};

struct ResamplerData2d
{
    PyBuffer x;
    PyBuffer y;
    PyBuffer z;
    QCPRange _x_range;
    QCPRange _plot_range;
    bool new_data = true;
};

template <bool data2d, typename U>
class _AbstractResampler
{
    using T = std::conditional_t<data2d, ResamplerData2d, ResamplerData1d>;
    QRecursiveMutex _data_mutex;
    QRecursiveMutex _next_data_mutex;
    T _data;
    T _next_data;

protected:
    inline void _async_resample_callback()
    {
        {
            QMutexLocker locker(&_data_mutex);
            {
                QMutexLocker locker(&_next_data_mutex);
                _data = _next_data;
                _next_data.new_data = false;
            }
            if constexpr (data2d)
            {
                static_cast<U*>(this)->_resample_impl(_data.x, _data.y, _data.z, _data._plot_range,
                                                      _data.new_data);
            }
            else
            {
                static_cast<U*>(this)->_resample_impl(_data.x, _data.y, _data._plot_range,
                                                      _data.new_data);
            }
        }
    }

    inline void set_next_plot_range(const QCPRange new_range)
    {
        QMutexLocker locker(&_next_data_mutex);
        _next_data._plot_range = new_range;
    }

public:
    template <typename... Args>
    inline void setData(PyBuffer x, Args... args)
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
            _next_data = T { std::move(x), std::move(args)..., _data_x_range, _data_x_range, true };
            static_cast<U*>(this)->resample(_data_x_range);
        }
    }

    inline QCPRange x_range()
    {
        QMutexLocker locker(&_data_mutex);
        return this->_data._x_range;
    }

    QList<PyBuffer> get_data()
    {
        if constexpr (data2d)
        {
            QMutexLocker locker(&_data_mutex);
            return { _data.x, _data.y, _data.z };
        }
        else
        {
            QMutexLocker locker(&_data_mutex);
            return { _data.x, _data.y };
        }
    }
};

struct AbstractResampler1d : public QObject, public _AbstractResampler<false, AbstractResampler1d>
{
    friend class _AbstractResampler<false, AbstractResampler1d>;

protected:
    Q_OBJECT
    std::size_t _line_cnt;

    Q_SLOT void _async_resample();

#ifndef BINDINGS_H
    Q_SIGNAL void _resample_sig();
#endif

    virtual void _resample_impl(const PyBuffer& x, const PyBuffer& y, const QCPRange new_range,
                                bool new_data)
        = 0;

public:
    AbstractResampler1d(std::size_t line_cnt);
    virtual ~AbstractResampler1d() = default;

    void resample(const QCPRange new_range);

    inline void set_line_count(std::size_t line_cnt) { this->_line_cnt = line_cnt; }

    inline std::size_t line_count() const { return _line_cnt; }
};

struct AbstractResampler2d : public QObject, public _AbstractResampler<true, AbstractResampler2d>
{
    friend class _AbstractResampler<true, AbstractResampler2d>;

protected:
    Q_OBJECT

    Q_SLOT void _async_resample();

#ifndef BINDINGS_H
    Q_SIGNAL void _resample_sig();
#endif

    virtual void _resample_impl(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
                                const QCPRange new_range, bool new_data)
        = 0;

public:
    AbstractResampler2d();
    virtual ~AbstractResampler2d() = default;

    void resample(const QCPRange new_range);
};

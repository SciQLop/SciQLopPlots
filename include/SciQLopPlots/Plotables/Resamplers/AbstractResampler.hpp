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
#include <SciQLopPlots/Plotables/SciQLopGraphInterface.hpp>
#include <SciQLopPlots/Python/PythonInterface.hpp>
#include <SciQLopPlots/Python/Views.hpp>
#include <SciQLopPlots/SciQLopPlotAxis.hpp>
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

struct ResamplerPlotInfo
{
    bool x_is_log = false;
    bool y_is_log = false;
    QSize plot_size;
    QCPRange plot_range;
};

struct ResamplerData1d
{
    PyBuffer x;
    PyBuffer y;
    QCPRange x_range;
    bool new_data = true;
};

inline XYView make_view(const ResamplerData1d& data, const ResamplerPlotInfo& plot_info)
{
    return XYView(data.x, data.y, plot_info.plot_range.lower, plot_info.plot_range.upper);
}

struct ResamplerData2d
{
    PyBuffer x;
    PyBuffer y;
    PyBuffer z;
    QCPRange x_range;
    bool new_data = true;
};

inline XYZView make_view(const ResamplerData2d& data, const ResamplerPlotInfo& plot_info)
{
    return XYZView(data.x, data.y, data.z, plot_info.plot_range.lower, plot_info.plot_range.upper);
}

template <bool data2d, typename U>
class _AbstractResampler
{
public:
    using data_t = std::conditional_t<data2d, ResamplerData2d, ResamplerData1d>;

private:
    QRecursiveMutex _data_mutex;
    QRecursiveMutex _next_data_mutex;
    data_t _data;
    data_t _next_data;
    QRecursiveMutex _plot_info_mutex;
    ResamplerPlotInfo _plot_info;

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
                static_cast<U*>(this)->_resample_impl(_data, _plot_info);
            }
            else
            {
                static_cast<U*>(this)->_resample_impl(_data, _plot_info);
            }
        }
    }

    inline void set_next_plot_range(const QCPRange new_range)
    {
        QMutexLocker locker(&_plot_info_mutex);
        _plot_info.plot_range = new_range;
    }

    QCPRange _bounds(const PyBuffer& b)
    {
        const auto len = b.flat_size();
        if (len > 0)
        {
            return { b.data()[0], b.data()[len - 1] };
        }
        return { std::nan(""), std::nan("") };
    }

    void _resample() { static_cast<U*>(this)->resample(_plot_info.plot_range); }

public:
    inline void setData(PyBuffer x, PyBuffer y, auto... maybe_z)
    {
        constexpr auto has_z = sizeof...(maybe_z) == 1;
        static_assert(!(data2d xor has_z), "Data must be 2D or 3D");
        {
            const QCPRange data_x_range = _bounds(x);
            QMutexLocker locker(&_next_data_mutex);
            _next_data
                = data_t { std::move(x), std::move(y), std::move(maybe_z)..., data_x_range, true };
            _resample();
        }
    }

    inline void set_x_scale_log(bool log)
    {
        {
            QMutexLocker locker(&_plot_info_mutex);
            _plot_info.x_is_log = log;
        }
        this->_resample();
    }

    inline void set_y_scale_log(bool log)
    {
        QMutexLocker locker(&_plot_info_mutex);
        _plot_info.y_is_log = log;
        this->_resample();
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

    void set_plot_size(QSize size)
    {
        QMutexLocker locker(&_plot_info_mutex);
        _plot_info.plot_size = size;
        this->_resample();
    }
};

struct AbstractResampler1d : public QObject, public _AbstractResampler<false, AbstractResampler1d>
{
private:
    friend class _AbstractResampler<false, AbstractResampler1d>;

protected:
    Q_OBJECT
    std::size_t _line_cnt;

    Q_SLOT void _async_resample();

    virtual void _resample_impl(const ResamplerData1d& data, const ResamplerPlotInfo& plot_info)
        = 0;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void _resample_sig();


public:
    AbstractResampler1d(SciQLopPlottableInterface* parent, std::size_t line_cnt);
    virtual ~AbstractResampler1d() = default;

    inline SciQLopPlottableInterface* parent_plottable() const
    {
        return static_cast<SciQLopPlottableInterface*>(QObject::parent());
    }

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

    virtual void _resample_impl(const ResamplerData2d& data, const ResamplerPlotInfo& plot_info)
        = 0;

public:
    AbstractResampler2d(SciQLopPlottableInterface* parent);
    virtual ~AbstractResampler2d() = default;

    inline SciQLopPlottableInterface* parent_plottable() const
    {
        return static_cast<SciQLopPlottableInterface*>(QObject::parent());
    }

    void resample(const QCPRange new_range);
};

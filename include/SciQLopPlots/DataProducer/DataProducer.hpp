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
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"


#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTimer>

struct _2D_data
{
    PyBuffer x;
    PyBuffer y;
};

struct _3D_data
{
    PyBuffer x;
    PyBuffer y;
    PyBuffer z;
};

using _NDdata = QList<PyBuffer>;

class DataProviderInterface : public QObject
{
    Q_OBJECT
    std::variant<SciQLopPlotRange, _2D_data, _3D_data, _NDdata> m_next_state;
    std::variant<SciQLopPlotRange, _2D_data, _3D_data, _NDdata> m_current_state;
    QTimer* m_rate_limit_timer;
    QMutex m_mutex;
    bool m_has_pending_change = false;

#ifndef BINDINGS_H
    Q_SIGNAL void _state_changed();
#endif
    Q_SLOT void _threaded_update();

    void _notify_new_data(const QList<PyBuffer>& data);

    void _range_based_update(const SciQLopPlotRange& new_range);
    void _data_based_update(const _2D_data& new_data);
    void _data_based_update(const _3D_data& new_data);
    void _data_based_update(const _NDdata& new_data);


    inline Q_SLOT void _start_timer()
    {
        if (!m_rate_limit_timer->isActive())
        {
            m_rate_limit_timer->start(100);
        }
    }

public:
    DataProviderInterface(QObject* parent = nullptr);
    virtual ~DataProviderInterface() = default;

    virtual QList<PyBuffer> get_data(double lower, double upper);
    virtual QList<PyBuffer> get_data(PyBuffer x, PyBuffer y);
    virtual QList<PyBuffer> get_data(PyBuffer x, PyBuffer y, PyBuffer z);
    virtual QList<PyBuffer> get_data(QList<PyBuffer> values);



#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void new_data_3d(PyBuffer x, PyBuffer y, PyBuffer z);
    Q_SIGNAL void new_data_2d(PyBuffer x, PyBuffer y);
    Q_SIGNAL void new_data_nd(QList<PyBuffer> values);

protected:
    void set_range(SciQLopPlotRange new_range) noexcept;
    void set_data(_2D_data new_data) noexcept;
    void set_data(_3D_data new_data) noexcept;
    void set_data(_NDdata new_data) noexcept;
    friend class DataProviderWorker;
};

class DataProviderWorker : public QObject
{
    Q_OBJECT
    QThread* m_worker_thread;
    DataProviderInterface* m_data_provider;

public:
    DataProviderWorker(QObject* parent = nullptr) : QObject(parent)
    {
        m_worker_thread = new QThread(this);
        m_worker_thread->start();
    }

    virtual ~DataProviderWorker();

    virtual void set_data_provider(DataProviderInterface* data_provider);

    inline Q_SLOT virtual void set_range(const SciQLopPlotRange& range)
    {
        m_data_provider->set_range(range);
    }

    inline Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y)
    {
        m_data_provider->set_data(_2D_data { x, y });
    }

    inline Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z)
    {
        m_data_provider->set_data(_3D_data { x, y, z });
    }

    inline Q_SLOT virtual void set_data(QList<PyBuffer> values)
    {
        m_data_provider->set_data(values);
    }
};


class SimplePyCallablePWrapper : public DataProviderInterface
{
    Q_OBJECT
    GetDataPyCallable m_callable;

public:
    SimplePyCallablePWrapper(GetDataPyCallable&& callable, QObject* parent = nullptr)
            : DataProviderInterface(parent), m_callable(std::move(callable))
    {
    }

    virtual ~SimplePyCallablePWrapper() = default;

    inline virtual QList<PyBuffer> get_data(double lower, double upper) override
    {
        auto r = m_callable.get_data(lower, upper);
        QList<PyBuffer> result;
        for (auto& a : r)
            result.emplace_back(std::move(a));
        return result;
    }

    inline virtual QList<PyBuffer> get_data(PyBuffer x, PyBuffer y) override
    {
        auto r = m_callable.get_data(x, y);
        QList<PyBuffer> result;
        for (auto& a : r)
            result.emplace_back(std::move(a));
        return result;
    }

    inline virtual QList<PyBuffer> get_data(PyBuffer x, PyBuffer y, PyBuffer z) override
    {
        auto r = m_callable.get_data(x, y, z);
        QList<PyBuffer> result;
        for (auto& a : r)
            result.emplace_back(std::move(a));
        return result;
    }

    inline void set_callable(GetDataPyCallable&& callable) { m_callable = std::move(callable); }
    inline GetDataPyCallable callable() const { return m_callable; }
};


class SimplePyCallablePipeline : public QObject
{
    Q_OBJECT
    SimplePyCallablePWrapper* m_callable_wrapper;
    DataProviderWorker* m_worker;

public:
    SimplePyCallablePipeline(GetDataPyCallable&& callable, QObject* parent = nullptr);

    virtual ~SimplePyCallablePipeline() = default;

    inline Q_SLOT void call(const SciQLopPlotRange& range) { m_worker->set_range(range); }
    inline Q_SLOT void call(PyBuffer x, PyBuffer y) { m_worker->set_data(x, y); }
    inline Q_SLOT void call(PyBuffer x, PyBuffer y, PyBuffer z) { m_worker->set_data(x, y, z); }
    inline Q_SLOT void call(QList<PyBuffer> values) { m_worker->set_data(values); }

    inline void set_callable(GetDataPyCallable&& callable) { m_callable_wrapper->set_callable(std::move(callable)); }
    inline GetDataPyCallable callable() const { return m_callable_wrapper->callable(); }


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void new_data_3d(PyBuffer x, PyBuffer y, PyBuffer z);
    Q_SIGNAL void new_data_2d(PyBuffer x, PyBuffer y);
    Q_SIGNAL void new_data_nd(QList<PyBuffer> values);
};

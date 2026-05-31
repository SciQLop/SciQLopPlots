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
#include <memory>

struct _2D_data
{
    SciQLopPyBuffer x;
    SciQLopPyBuffer y;
};

struct _3D_data
{
    SciQLopPyBuffer x;
    SciQLopPyBuffer y;
    SciQLopPyBuffer z;
};

using _NDdata = QList<SciQLopPyBuffer>;

class DataProviderInterface : public QObject
{
    Q_OBJECT
    // Range-based and data-based updates use independent pending slots so a
    // call(range) and a call(x,y) racing on the same provider never overwrite
    // each other (each is serviced; neither is dropped). A single shared slot
    // used to let the second caller clobber the first and suppress its wake-up.
    SciQLopPlotRange m_next_range;
    std::variant<std::monostate, _2D_data, _3D_data, _NDdata> m_next_data;
    SciQLopPlotRange m_current_range;
    QTimer* m_rate_limit_timer;
    QMutex m_mutex;
    bool m_range_pending = false;
    bool m_data_pending = false;
    bool m_force_next_update = false;

#ifndef BINDINGS_H
    Q_SIGNAL void _state_changed();
#endif
    Q_SLOT void _threaded_update();

    void _notify_new_data(const QList<SciQLopPyBuffer>& data);

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

    virtual QList<SciQLopPyBuffer> get_data(double lower, double upper);
    virtual QList<SciQLopPyBuffer> get_data(SciQLopPyBuffer x, SciQLopPyBuffer y);
    virtual QList<SciQLopPyBuffer> get_data(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z);
    virtual QList<SciQLopPyBuffer> get_data(QList<SciQLopPyBuffer> values);

    void invalidate_cache();



#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void new_data_3d(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z);
    Q_SIGNAL void new_data_2d(SciQLopPyBuffer x, SciQLopPyBuffer y);
    Q_SIGNAL void new_data_nd(QList<SciQLopPyBuffer> values);
    Q_SIGNAL void pipeline_idle();

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

    inline Q_SLOT virtual void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
    {
        m_data_provider->set_data(_2D_data { x, y });
    }

    inline Q_SLOT virtual void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z)
    {
        m_data_provider->set_data(_3D_data { x, y, z });
    }

    inline Q_SLOT virtual void set_data(QList<SciQLopPyBuffer> values)
    {
        m_data_provider->set_data(values);
    }
};


class SimplePyCallablePWrapper : public DataProviderInterface
{
    Q_OBJECT
    // Held by shared_ptr so the worker can take a stable snapshot under the
    // mutex with a pure (C++ atomic) refcount bump — NO Python incref, so the
    // GIL is never acquired while m_callable_mutex is held. Copying the callable
    // directly used to incref under the lock (mutex→GIL), which deadlocks
    // against set_callable()/callable() called from Python (GIL→mutex). The
    // snapshot also lets us call into the old callable safely if it is swapped
    // out concurrently: the snapshot keeps it alive until the call returns.
    std::shared_ptr<GetDataPyCallable> m_callable;
    mutable QMutex m_callable_mutex;

    inline std::shared_ptr<GetDataPyCallable> _snapshot_callable() const
    {
        QMutexLocker lock(&m_callable_mutex);
        return m_callable; // atomic refcount bump, no GIL
    }

    static inline QList<SciQLopPyBuffer> _to_qlist(std::vector<SciQLopPyBuffer>&& r)
    {
        QList<SciQLopPyBuffer> result;
        for (auto& a : r)
            result.emplace_back(std::move(a));
        return result;
    }

public:
    SimplePyCallablePWrapper(GetDataPyCallable&& callable, QObject* parent = nullptr)
            : DataProviderInterface(parent)
            , m_callable(std::make_shared<GetDataPyCallable>(std::move(callable)))
    {
    }

    virtual ~SimplePyCallablePWrapper() = default;

    inline virtual QList<SciQLopPyBuffer> get_data(double lower, double upper) override
    {
        auto cb = _snapshot_callable();
        return cb ? _to_qlist(cb->get_data(lower, upper)) : QList<SciQLopPyBuffer> {};
    }

    inline virtual QList<SciQLopPyBuffer> get_data(SciQLopPyBuffer x, SciQLopPyBuffer y) override
    {
        auto cb = _snapshot_callable();
        return cb ? _to_qlist(cb->get_data(x, y)) : QList<SciQLopPyBuffer> {};
    }

    inline virtual QList<SciQLopPyBuffer> get_data(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z) override
    {
        auto cb = _snapshot_callable();
        return cb ? _to_qlist(cb->get_data(x, y, z)) : QList<SciQLopPyBuffer> {};
    }

    inline void set_callable(GetDataPyCallable&& callable)
    {
        auto next = std::make_shared<GetDataPyCallable>(std::move(callable));
        QMutexLocker lock(&m_callable_mutex);
        m_callable.swap(next);
        // `lock` (declared last) unlocks before `next` (the old callable) is
        // destroyed at scope exit, so the old callable's Python decref happens
        // outside the mutex — and defers if this thread lacks the GIL.
    }

    inline GetDataPyCallable callable() const
    {
        auto cb = _snapshot_callable();
        // Copy (Python incref) happens here, OUTSIDE the mutex — never mutex→GIL.
        return cb ? *cb : GetDataPyCallable();
    }
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
    inline Q_SLOT void call(SciQLopPyBuffer x, SciQLopPyBuffer y) { m_worker->set_data(x, y); }
    inline Q_SLOT void call(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z) { m_worker->set_data(x, y, z); }
    inline Q_SLOT void call(QList<SciQLopPyBuffer> values) { m_worker->set_data(values); }

    inline void set_callable(GetDataPyCallable&& callable) { m_callable_wrapper->set_callable(std::move(callable)); }
    inline GetDataPyCallable callable() const { return m_callable_wrapper->callable(); }

    inline void invalidate_cache() { m_callable_wrapper->invalidate_cache(); }


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void new_data_3d(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z);
    Q_SIGNAL void new_data_2d(SciQLopPyBuffer x, SciQLopPyBuffer y);
    Q_SIGNAL void new_data_nd(QList<SciQLopPyBuffer> values);
    Q_SIGNAL void pipeline_idle();
};

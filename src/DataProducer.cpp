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
#include "SciQLopPlots/DataProducer/DataProducer.hpp"
#include <iostream>
#include "SciQLopPlots/Debug.hpp"


void DataProviderInterface::_threaded_update()
{
    SciQLopPlotRange range;
    bool do_range = false;
    std::variant<std::monostate, _2D_data, _3D_data, _NDdata> data;
    bool do_data = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_range_pending)
        {
            range = m_next_range;
            m_range_pending = false;
            do_range = true;
        }
        if (m_data_pending)
        {
            data = m_next_data;
            m_data_pending = false;
            do_data = true;
        }
    }

    if (do_range)
        _range_based_update(range);
    if (do_data)
        std::visit(
            [this](auto&& d)
            {
                if constexpr (!std::is_same_v<std::decay_t<decltype(d)>, std::monostate>)
                    _data_based_update(d);
            },
            data);

    bool idle = false;
    {
        QMutexLocker lock(&m_mutex);
        idle = !m_range_pending && !m_data_pending;
    }
    if (idle)
        Q_EMIT pipeline_idle();
}

void DataProviderInterface::_notify_new_data(const QList<SciQLopPyBuffer> &data)
{
    if (data.size() == 2)
    {
        Q_EMIT new_data_2d(data[0], data[1]);
    }
    else if (data.size() == 3)
    {
        Q_EMIT new_data_3d(data[0], data[1], data[2]);
    }
    else if (data.size() != 0)
    {
        Q_EMIT new_data_nd(data);
    }
}

void DataProviderInterface::_range_based_update(const SciQLopPlotRange& new_range)
{
    bool force;
    {
        QMutexLocker lock(&m_mutex);
        force = m_force_next_update;
        m_force_next_update = false;
    }
    if (!force && new_range == m_current_range)
        return;
    auto r = get_data(new_range.start(), new_range.stop());
    m_current_range = new_range;
    _notify_new_data(r);
}

void DataProviderInterface::invalidate_cache()
{
    QMutexLocker lock(&m_mutex);
    m_force_next_update = true;
}

void DataProviderInterface::_data_based_update(const _2D_data& new_data)
{
    _notify_new_data(get_data(new_data.x, new_data.y));
}

void DataProviderInterface::_data_based_update(const _3D_data& new_data)
{
   _notify_new_data(get_data(new_data.x, new_data.y, new_data.z));
}

void DataProviderInterface::_data_based_update(const _NDdata &new_data)
{
    _notify_new_data(get_data(new_data));
}


DataProviderInterface::DataProviderInterface(QObject* parent) : QObject(parent)
{
    m_rate_limit_timer = new QTimer(this);
    m_rate_limit_timer->setSingleShot(true);
    m_rate_limit_timer->setInterval(20);
    connect(m_rate_limit_timer, &QTimer::timeout, this, &DataProviderInterface::_threaded_update);
    connect(this, &DataProviderInterface::_state_changed, this,
        &DataProviderInterface::_threaded_update, Qt::QueuedConnection);
}

QList<SciQLopPyBuffer> DataProviderInterface::get_data(double lower, double upper)
{
    WARN_ABSTRACT_METHOD;
    return { {}, {}, {} };
}

QList<SciQLopPyBuffer> DataProviderInterface::get_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
{
    WARN_ABSTRACT_METHOD;
    return { {}, {}, {} };
}

QList<SciQLopPyBuffer> DataProviderInterface::get_data(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z)
{
    WARN_ABSTRACT_METHOD;
    return { {}, {}, {} };
}

QList<SciQLopPyBuffer> DataProviderInterface::get_data(QList<SciQLopPyBuffer> values)
{
    WARN_ABSTRACT_METHOD;
    return { {}, {}, {} , {} };
}

void DataProviderInterface::set_range(SciQLopPlotRange new_state) noexcept
{
    {
        QMutexLocker lock(&m_mutex);
        m_next_range = new_state;
        m_range_pending = true;
    }
    // Range fetches are rate-limited (panning spams them): the timer coalesces
    // the wake-up. _threaded_update then services whichever slots are pending.
    QMetaObject::invokeMethod(m_rate_limit_timer, qOverload<>(&QTimer::start),
                              Qt::QueuedConnection);
}

// The data setters wake on their OWN pending flag only, independently of a
// pending range fetch — so a range fetch in flight no longer suppresses the
// data wake-up (and vice-versa). Consecutive data calls still coalesce.
void DataProviderInterface::set_data(_2D_data new_state) noexcept
{
    bool should_emit = false;
    {
        QMutexLocker lock(&m_mutex);
        m_next_data = new_state;
        if (!m_data_pending)
        {
            m_data_pending = true;
            should_emit = true;
        }
    }
    if (should_emit)
        Q_EMIT _state_changed();
}

void DataProviderInterface::set_data(_3D_data new_state) noexcept
{
    bool should_emit = false;
    {
        QMutexLocker lock(&m_mutex);
        m_next_data = new_state;
        if (!m_data_pending)
        {
            m_data_pending = true;
            should_emit = true;
        }
    }
    if (should_emit)
        Q_EMIT _state_changed();
}

void DataProviderInterface::set_data(_NDdata new_state) noexcept
{
    bool should_emit = false;
    {
        QMutexLocker lock(&m_mutex);
        m_next_data = new_state;
        if (!m_data_pending)
        {
            m_data_pending = true;
            should_emit = true;
        }
    }
    if (should_emit)
        Q_EMIT _state_changed();
}

DataProviderWorker::~DataProviderWorker()
{
    m_worker_thread->quit();
    m_worker_thread->wait();
}

void DataProviderWorker::set_data_provider(DataProviderInterface* data_provider)
{
    data_provider->setParent(nullptr);
    m_data_provider = data_provider;
    m_data_provider->moveToThread(m_worker_thread);
    connect(m_worker_thread, &QThread::finished, m_data_provider, &QObject::deleteLater);
    connect(m_worker_thread, &QThread::finished, m_worker_thread, &QObject::deleteLater);
}

SimplePyCallablePipeline::SimplePyCallablePipeline(GetDataPyCallable&& callable, QObject* parent)
        : QObject(parent)
{
    m_callable_wrapper = new SimplePyCallablePWrapper(std::move(callable), this);
    m_worker = new DataProviderWorker(this);
    m_worker->set_data_provider(m_callable_wrapper);
    connect(m_callable_wrapper, &SimplePyCallablePWrapper::new_data_2d, this,
        &SimplePyCallablePipeline::new_data_2d);
    connect(m_callable_wrapper, &SimplePyCallablePWrapper::new_data_3d, this,
        &SimplePyCallablePipeline::new_data_3d);
    connect(m_callable_wrapper, &SimplePyCallablePWrapper::new_data_nd, this,
        &SimplePyCallablePipeline::new_data_nd);
    connect(m_callable_wrapper, &SimplePyCallablePWrapper::pipeline_idle, this,
        &SimplePyCallablePipeline::pipeline_idle);
}

RemoteDataPipeline::RemoteDataPipeline(QObject* parent) : QObject(parent)
{
    m_provider = new RemoteDataProvider(this);
    m_worker = new DataProviderWorker(this);
    m_worker->set_data_provider(m_provider);
    connect(m_provider, &RemoteDataProvider::data_requested, this,
            &RemoteDataPipeline::data_requested);
    connect(m_provider, &RemoteDataProvider::new_data_2d, this,
            &RemoteDataPipeline::new_data_2d);
    connect(m_provider, &RemoteDataProvider::new_data_3d, this,
            &RemoteDataPipeline::new_data_3d);
    connect(m_provider, &RemoteDataProvider::new_data_nd, this,
            &RemoteDataPipeline::new_data_nd);
    connect(m_provider, &RemoteDataProvider::pipeline_idle, this,
            &RemoteDataPipeline::pipeline_idle);
}

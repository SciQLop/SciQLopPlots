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
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <numeric>
#include "SciQLopPlots/Debug.hpp"

namespace
{
std::size_t byte_size(const QList<SciQLopPyBuffer>& buffers)
{
    return std::transform_reduce(buffers.cbegin(), buffers.cend(), std::size_t { 0 },
                                 std::plus<> {}, [](const SciQLopPyBuffer& b)
                                 { return b.is_valid() ? b.flat_size() * b.item_size() : 0; });
}

// The margin cut so the whole fetch fits in budget bytes; the view itself is always fetched.
double margin_within_budget(double margin, double budget, double bytes_per_key, double view_span)
{
    if (budget <= 0. || bytes_per_key <= 0. || view_span <= 0.)
        return margin;
    return std::clamp((budget / (bytes_per_key * view_span) - 1.) / 2., 0., margin);
}

double non_negative_or_zero(double v)
{
    return std::isfinite(v) ? std::max(0., v) : 0.;
}
}


void DataProviderInterface::_threaded_update()
{
    SciQLopPlotRange range;
    bool do_range = false;
    _PendingData data;
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

// Remote data lands here long after its request: m_current_range is still the range asked for.
void DataProviderInterface::_record_density(std::size_t bytes)
{
    // An empty answer (a data gap) says nothing about the product's density.
    if (const double span = m_current_range.size(); bytes > 0 && span > 0.)
        m_bytes_per_key = static_cast<double>(bytes) / span;
}

void DataProviderInterface::_notify_new_data(const QList<SciQLopPyBuffer> &data)
{
    _record_density(byte_size(data));
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

void DataProviderInterface::_notify_new_data(const _Colored_data& batch)
{
    if (!batch.color.is_valid())
        _notify_new_data(batch.data);
    else if (!batch.data.isEmpty())
    {
        _record_density(byte_size(batch.data) + byte_size({ batch.color }));
        Q_EMIT new_data_colored(batch.data, batch.color);
    }
}

bool DataProviderInterface::_is_loaded(const SciQLopPlotRange& view, double margin) const
{
    return margin > 0. ? m_current_range.contains(view) : view == m_current_range;
}

void DataProviderInterface::_range_based_update(const SciQLopPlotRange& new_range)
{
    bool force;
    double margin;
    double budget;
    {
        QMutexLocker lock(&m_mutex);
        force = m_force_next_update;
        m_force_next_update = false;
        margin = m_prefetch_margin;
        budget = m_prefetch_budget_bytes;
    }
    if (!force && _is_loaded(new_range, margin))
    {
        Q_EMIT request_ended();
        return;
    }
    const double widening = margin_within_budget(margin, budget, m_bytes_per_key, new_range.size());
    const auto wanted = widening > 0. ? new_range * (1. + 2. * widening) : new_range;
    auto r = fetch(wanted.start(), wanted.stop());
    m_current_range = wanted;
    _notify_new_data(r);
}

void DataProviderInterface::invalidate_cache()
{
    QMutexLocker lock(&m_mutex);
    m_force_next_update = true;
}

void DataProviderInterface::set_prefetch_margin(double margin)
{
    QMutexLocker lock(&m_mutex);
    m_prefetch_margin = non_negative_or_zero(margin);
}

double DataProviderInterface::prefetch_margin() const
{
    QMutexLocker lock(&m_mutex);
    return m_prefetch_margin;
}

void DataProviderInterface::set_prefetch_budget_bytes(double bytes)
{
    QMutexLocker lock(&m_mutex);
    m_prefetch_budget_bytes = non_negative_or_zero(bytes);
}

double DataProviderInterface::prefetch_budget_bytes() const
{
    QMutexLocker lock(&m_mutex);
    return m_prefetch_budget_bytes;
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
    const auto result = get_data(new_data);
    if (result.isEmpty())
        Q_EMIT request_ended();
    else
        _notify_new_data(result);
}

// Pushed as-is: a coloured batch is already final (only the remote channel sends one).
void DataProviderInterface::_data_based_update(const _Colored_data& new_data)
{
    if (new_data.data.isEmpty())
        Q_EMIT request_ended();
    else
        _notify_new_data(new_data);
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
void DataProviderInterface::_queue_data(_PendingData new_state) noexcept
{
    bool should_emit = false;
    {
        QMutexLocker lock(&m_mutex);
        m_next_data = std::move(new_state);
        if (!m_data_pending)
        {
            m_data_pending = true;
            should_emit = true;
        }
    }
    if (should_emit)
        Q_EMIT _state_changed();
}

void DataProviderInterface::set_data(_2D_data new_state) noexcept
{
    _queue_data(std::move(new_state));
}

void DataProviderInterface::set_data(_3D_data new_state) noexcept
{
    _queue_data(std::move(new_state));
}

void DataProviderInterface::set_data(_NDdata new_state) noexcept
{
    _queue_data(std::move(new_state));
}

void DataProviderInterface::set_data(_Colored_data new_state) noexcept
{
    _queue_data(std::move(new_state));
}

// No join: the worker may be inside a long Python callback, and quit() only takes effect
// once its event loop gets control back, which would freeze the GUI thread that destroys
// us for as long as the callback lasts. The thread frees itself when it finishes, so it
// must not be our child (a running QThread must not be deleted). Whoever owns the provider
// disconnects its signals first, so a late result is dropped.
DataProviderWorker::~DataProviderWorker()
{
    m_worker_thread->setParent(nullptr);
    m_worker_thread->quit();
}

void DataProviderWorker::set_data_provider(DataProviderInterface* data_provider)
{
    data_provider->setParent(nullptr);
    m_data_provider = data_provider;
    m_data_provider->moveToThread(m_worker_thread);
    connect(m_worker_thread, &QThread::finished, m_data_provider, &QObject::deleteLater);
}

SimplePyCallablePipeline::~SimplePyCallablePipeline()
{
    QObject::disconnect(m_callable_wrapper, nullptr, this, nullptr);
}

RemoteDataPipeline::~RemoteDataPipeline()
{
    QObject::disconnect(m_provider, nullptr, this, nullptr);
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
    connect(m_callable_wrapper, &SimplePyCallablePWrapper::new_data_colored, this,
        &SimplePyCallablePipeline::new_data_colored);
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
    connect(m_provider, &RemoteDataProvider::new_data_colored, this,
            &RemoteDataPipeline::new_data_colored);
    connect(m_provider, &RemoteDataProvider::request_ended, this,
            &RemoteDataPipeline::request_ended);
    connect(m_provider, &RemoteDataProvider::pipeline_idle, this,
            &RemoteDataPipeline::pipeline_idle);
}

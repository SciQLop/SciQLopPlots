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


void DataProviderInterface::_threaded_update()
{
    m_mutex.lock();
    if (std::holds_alternative<SciQLopPlotRange>(m_next_state))
    {
        auto new_range = std::get<SciQLopPlotRange>(m_next_state);
        m_has_pending_change = false;
        m_mutex.unlock();
        _range_based_update(new_range);
    }
    else if (std::holds_alternative<_2D_data>(m_next_state))
    {
        auto new_data = std::get<_2D_data>(m_next_state);
        m_has_pending_change = false;
        m_mutex.unlock();
        _data_based_update(new_data);
    }
    else if (std::holds_alternative<_3D_data>(m_next_state))
    {
        auto new_data = std::get<_3D_data>(m_next_state);
        m_has_pending_change = false;
        m_mutex.unlock();
        _data_based_update(new_data);
    }
    else
    {
        m_mutex.unlock();
    }
}

void DataProviderInterface::_range_based_update(const SciQLopPlotRange& new_range)
{
    if (new_range == std::get<SciQLopPlotRange>(m_current_state))
        return;
    auto r = get_data(new_range.start(), new_range.stop());
    m_current_state = new_range;
    if (r.size() == 2)
    {
        Q_EMIT new_data_2d(r[0], r[1]);
    }
    else if (r.size() == 3)
    {
        Q_EMIT new_data_3d(r[0], r[1], r[2]);
    }
    else if (r.size() != 0)
    {
        std::cerr << "Data provider returned invalid data" << std::endl;
    }
}

void DataProviderInterface::_data_based_update(const _2D_data& new_data)
{
    auto r = get_data(new_data.x, new_data.y);
    if (r.size() == 2)
    {
        Q_EMIT new_data_2d(r[0], r[1]);
    }
    else if (r.size() == 3)
    {
        Q_EMIT new_data_3d(r[0], r[1], r[2]);
    }
    else if (r.size() != 0)
    {
        std::cerr << "Data provider returned invalid data" << std::endl;
    }
}

void DataProviderInterface::_data_based_update(const _3D_data& new_data)
{
    auto r = get_data(new_data.x, new_data.y, new_data.z);
    if (r.size() == 2)
    {
        Q_EMIT new_data_2d(r[0], r[1]);
    }
    else if (r.size() == 3)
    {
        Q_EMIT new_data_3d(r[0], r[1], r[2]);
    }
    else if (r.size() != 0)
    {
        std::cerr << "Data provider returned invalid data" << std::endl;
    }
}


DataProviderInterface::DataProviderInterface(QObject* parent) : QObject(parent)
{
    m_rate_limit_timer = new QTimer(this);
    m_rate_limit_timer->setSingleShot(true);
    m_rate_limit_timer->setInterval(100);
    connect(m_rate_limit_timer, &QTimer::timeout, this, &DataProviderInterface::_threaded_update);
    connect(this, &DataProviderInterface::_state_changed, this,
        &DataProviderInterface::_threaded_update, Qt::QueuedConnection);
}

QList<PyBuffer> DataProviderInterface::get_data(double lower, double upper)
{
    std::cout << "Not implemented please implement in derived class" << std::endl;
    return { {}, {}, {} };
}

QList<PyBuffer> DataProviderInterface::get_data(PyBuffer x, PyBuffer y)
{
    std::cout << "Not implemented please implement in derived class" << std::endl;
    return { {}, {}, {} };
}

QList<PyBuffer> DataProviderInterface::get_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    std::cout << "Not implemented please implement in derived class" << std::endl;
    return { {}, {}, {} };
}

void DataProviderInterface::set_range(SciQLopPlotRange new_state) noexcept
{
    m_mutex.lock();
    m_next_state = new_state;
    if (!m_has_pending_change)
    {
        m_has_pending_change = true;
        Q_EMIT _state_changed();
    }
    m_mutex.unlock();
}

void DataProviderInterface::set_data(_2D_data new_state) noexcept
{
    m_mutex.lock();
    m_next_state = new_state;
    if (!m_has_pending_change)
    {
        m_has_pending_change = true;
        Q_EMIT _state_changed();
    }
    m_mutex.unlock();
}

void DataProviderInterface::set_data(_3D_data new_state) noexcept
{
    m_mutex.lock();
    m_next_state = new_state;
    if (!m_has_pending_change)
    {
        m_has_pending_change = true;
        Q_EMIT _state_changed();
    }
    m_mutex.unlock();
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
{
    m_callable_wrapper = new SimplePyCallablePWrapper(std::move(callable), this);
    m_worker = new DataProviderWorker(this);
    m_worker->set_data_provider(m_callable_wrapper);
    connect(m_callable_wrapper, &SimplePyCallablePWrapper::new_data_2d, this,
        &SimplePyCallablePipeline::new_data_2d);
    connect(m_callable_wrapper, &SimplePyCallablePWrapper::new_data_3d, this,
        &SimplePyCallablePipeline::new_data_3d);
}

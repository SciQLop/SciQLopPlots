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
    auto new_range = m_next_range;
    m_has_pending_range = false;
    m_mutex.unlock();
    if (new_range == m_current_range)
        return;
    auto r = get_data(new_range.lower, new_range.upper);
    m_current_range = new_range;
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
    connect(this, &DataProviderInterface::_range_changed, this,
        &DataProviderInterface::_threaded_update, Qt::QueuedConnection);
}

QList<PyBuffer> DataProviderInterface::get_data(double lower, double upper)
{
    std::cout << "Not implemented please implement in derived class" << std::endl;
    return { {}, {}, {} };
}

void DataProviderInterface::test_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    std::cout << "X size: " << x.size() << std::endl;
    std::cout << "Y size: " << y.size() << std::endl;
    std::cout << "Z size: " << z.size() << std::endl;
}

void DataProviderInterface::set_range(_TrivialRange new_range) noexcept
{
    m_mutex.lock();
    m_next_range = new_range;
    if (!m_has_pending_range)
    {
        m_has_pending_range = true;
        Q_EMIT _range_changed();
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

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
#include "SciQLopPlots/Python/BufferProtocol.hpp"


#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTimer>


struct _TrivialRange
{
    double lower;
    double upper;
    bool operator==(const _TrivialRange& other) const noexcept
    {
        return lower == other.lower && upper == other.upper;
    }
};

class DataProviderInterface : public QObject
{
    Q_OBJECT
    _TrivialRange m_next_range;
    _TrivialRange m_current_range;
    QTimer* m_rate_limit_timer;
    QMutex m_mutex;
    bool m_has_pending_range = false;

#ifndef BINDINGS_H
    Q_SIGNAL void _range_changed();
#endif
    Q_SLOT void _threaded_update();
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

    virtual QList<Array_view> get_data(double lower, double upper);

    void test_data(Array_view x, Array_view y, Array_view z);

#ifndef BINDINGS_H
    Q_SIGNAL void new_data_3d(Array_view x, Array_view y, Array_view z);
    Q_SIGNAL void new_data_2d(Array_view x, Array_view y);
#endif

protected:
    void set_range(_TrivialRange new_range) noexcept;
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

    Q_SLOT virtual void set_range(double lower, double upper)
    {
        m_data_provider->set_range({ lower, upper });
    }
};

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
#include <qcustomplot.h>

class SQPQCPAbstractPlottableWrapper : public QObject
{
    Q_OBJECT
protected:
    QList<QCPAbstractPlottable*> m_plottables;
    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

public:
    SQPQCPAbstractPlottableWrapper(QCustomPlot* parent) : QObject(parent) { }
    virtual ~SQPQCPAbstractPlottableWrapper()
    {
        for (auto plottable : m_plottables)
        {
            if (_plot()->hasPlottable(plottable))
                _plot()->removePlottable(plottable);
        }
    }

    inline void clear_plottables()
    {
        for (auto plottable : m_plottables)
        {
            if (_plot()->hasPlottable(plottable))
                _plot()->removePlottable(plottable);
        }
        m_plottables.clear();
    }

    QList<QCPAbstractPlottable*> plottables() const noexcept { return m_plottables; }

    template <typename T>
    inline T* newPlottable(QCPAxis* keyAxis, QCPAxis* valueAxis, const QString& name)
    {
        QCPAbstractPlottable* plottable = nullptr;
        if constexpr (std::is_same_v<T, QCPGraph>)
        {
            plottable = _plot()->addGraph(keyAxis, valueAxis);
        }
        else if constexpr (std::is_same_v<T, QCPColorMap>)
        {
            plottable = new QCPColorMap(keyAxis, valueAxis);
        }
        else if constexpr (std::is_same_v<T, QCPCurve>)
        {
            plottable = new QCPCurve(keyAxis, valueAxis);
        }
        else
            return nullptr;

        connect(plottable, &QCPAbstractPlottable::destroyed, this,
            [this, plottable]() { m_plottables.removeOne(plottable); });
        m_plottables.append(plottable);
#ifndef BINDINGS_H
        emit plottable_created(plottable);
#endif
        plottable->setName(name);
        return reinterpret_cast<T*>(plottable);
    }

    std::size_t plottable_count() const noexcept { return std::size(m_plottables); }

#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(const QCPRange& newRange, bool missData);
    Q_SIGNAL void plottable_created(QCPAbstractPlottable*);
#endif
};

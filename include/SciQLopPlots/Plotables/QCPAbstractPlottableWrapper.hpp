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

#include "SciQLopPlots/Plotables/SciQLopGraphComponent.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"

template <typename T>
inline void _set_selected(T* plottable, bool selected)
{
    if (plottable->selected() != selected)
    {
        if (selected)
            plottable->setSelection(QCPDataSelection(plottable->data()->dataRange()));
        else
            plottable->setSelection(QCPDataSelection());
    }
}

inline void set_selected(QCPAbstractPlottable* plottable, bool selected)
{
    if (auto graph = dynamic_cast<QCPGraph*>(plottable); graph != nullptr)
    {
        _set_selected(graph, selected);
    }
    else if (auto curve = dynamic_cast<QCPCurve*>(plottable); curve != nullptr)
    {
        _set_selected(curve, selected);
    }
}

class SQPQCPAbstractPlottableWrapper : public SciQLopGraphInterface
{
    Q_OBJECT

protected:
    QList<QPointer<SciQLopGraphComponent>> m_components;

    void _register_component(SciQLopGraphComponent* component);

public:
    SQPQCPAbstractPlottableWrapper(const QString& prefix, QVariantMap metaData, QCustomPlot* parent)
            : SciQLopGraphInterface(prefix, metaData, parent)
    {
    }

    virtual ~SQPQCPAbstractPlottableWrapper() { clear_plottables(); }

    inline void clear_plottables()
    {
        for (auto component : m_components)
            delete component;
        m_components.clear();
    }

    const QList<QCPAbstractPlottable*> qcp_plottables() const noexcept
    {
        QList<QCPAbstractPlottable*> plottables;
        for (auto component : m_components)
            plottables.append(component->plottable());
        return plottables;
    }

    template <typename T>
    inline SciQLopGraphComponent* newComponent(QCPAxis* keyAxis, QCPAxis* valueAxis,
                                               const QString& name)
    {
        auto plot = keyAxis->parentPlot();
        SciQLopGraphComponent* component = nullptr;
        if constexpr (std::is_same_v<T, QCPGraph>)
        {
            component = new SciQLopGraphComponent(plot->addGraph(keyAxis, valueAxis), this);
        }
        else if constexpr (std::is_same_v<T, QCPCurve>)
        {
            component = new SciQLopGraphComponent(new QCPCurve(keyAxis, valueAxis), this);
        }
        if (component)
        {
            component->set_name(name);
            _register_component(component);
        }
        return component;
    }

    std::size_t plottable_count() const noexcept { return std::size(m_components); }

    virtual void set_visible(bool visible) noexcept override;
    virtual void set_labels(const QStringList& labels) override;
    virtual void set_colors(const QList<QColor>& colors) override;

    virtual bool visible() const noexcept override;
    virtual QStringList labels() const noexcept override;

    virtual void set_selected(bool selected) noexcept override;
    virtual bool selected() const noexcept override;

    virtual SciQLopGraphComponentInterface* component(const QString& name) const noexcept override
    {
        for (auto component : m_components)
        {
            if (component->name() == name)
                return component;
        }
        return nullptr;
    }

    virtual SciQLopGraphComponentInterface* component(int index) const noexcept override
    {
        if (index == -1 || static_cast<std::size_t>(index) >= plottable_count())
            index = plottable_count() - 1;
        return m_components[index];
    }

    virtual QList<SciQLopGraphComponentInterface*> components() const noexcept override
    {
        QList<SciQLopGraphComponentInterface*> components;
        components.reserve(std::size(m_components));
        for (auto component : m_components)
            components.append(component);
        return components;
    }


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void plottable_created(QCPAbstractPlottable*);

};

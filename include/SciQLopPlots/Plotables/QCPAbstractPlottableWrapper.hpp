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

class SQPQCPAbstractPlottableWrapper : public SciQLopGraphInterface
{
    Q_OBJECT

    // The graph's last state, kept for components still to come: a callback graph has none
    // until its first data, and a graph hidden before then must come up hidden.
    bool m_visible_state = true;
    bool m_selected_state = false;

    void _apply_graph_state(SciQLopGraphComponent* component);
    void _track_parts();

protected:
    QList<QPointer<SciQLopGraphComponent>> m_components;

    void _register_component(SciQLopGraphComponent* component);
    void _forget_destroyed_components();
    void _seed_parts();

    //! Detaches the components from \a first on before deleting any: a deleting one makes the
    //! owner (a multigraph) shrink its own array first, so a wrapper left listed would read past
    //! its end. Disconnected too: a dying component signals, and this graph may be half destroyed
    //! (the destructor) or about to rebuild them (create_graphs, a shrinking set_data).
    inline void _drop_components_from(qsizetype first)
    {
        const auto gone = m_components.sliced(first);
        m_components.resize(first);
        for (const auto& component : gone)
            if (component)
            {
                disconnect(component, nullptr, this, nullptr);
                delete component.data();
            }
        _seed_parts();
    }

#ifndef BINDINGS_H
    //! Tells the plot to re-read its colour scale; a gradient is requested on it too.
    void notify_color_scale(std::optional<::ColorGradient> gradient = std::nullopt);
    //! Same, once this graph has left the plot's list: for destructors.
    void notify_color_scale_later();
#endif

public:
    SQPQCPAbstractPlottableWrapper(const QString& prefix, QVariantMap metaData, QCustomPlot* parent)
            : SciQLopGraphInterface(prefix, metaData, parent)
    {
    }

    virtual ~SQPQCPAbstractPlottableWrapper() { clear_plottables(); }

    inline void clear_plottables() { _drop_components_from(0); }

    const QList<QCPAbstractPlottable*> qcp_plottables() const noexcept
    {
        QList<QCPAbstractPlottable*> plottables;
        for (auto component : m_components)
        {
            auto p = component->plottable();
            if (p && !plottables.contains(p))
                plottables.append(p);
        }
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
        else if constexpr (std::is_base_of_v<QCPCurve, T>)
        {
            component = new SciQLopGraphComponent(new T(keyAxis, valueAxis), this);
        }
        if (component)
        {
            component->set_name(name);
            _register_component(component);
        }
        return component;
    }

    std::size_t plottable_count() const noexcept { return std::size(m_components); }

    virtual void set_labels(const QStringList& labels) override;
    virtual void set_colors(const QList<QColor>& colors) override;

    virtual QStringList labels() const noexcept override;
    virtual QList<QColor> colors() const noexcept override;

    virtual bool busy() const noexcept override
    {
        for (auto component : m_components)
        {
            auto p = component->plottable();
            if (p && p->busy())
                return true;
        }
        return false;
    }

    virtual void set_busy(bool busy) noexcept override
    {
        // A busy_changed slot may delete this graph and its components: walk a snapshot.
        const QPointer<QObject> self(this);
        const auto parts = m_components;
        for (const auto& part : parts)
        {
            if (!self)
                return;
            if (auto* p = part ? part->plottable() : nullptr)
                p->setBusy(busy);
        }
    }

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
        if (m_components.isEmpty())
            return nullptr;
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

protected:
    bool _always_apply() const override { return true; }
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override;
    void _set_selected(bool selected) override;

public:
#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void plottable_created(QCPAbstractPlottable*);

};

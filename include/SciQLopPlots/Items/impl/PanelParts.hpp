/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#pragma once

#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"

#include <QList>
#include <QPointer>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace impl
{
//! \a panel, checked, for a panel-scope object's first base-class initializer.
inline SciQLopMultiPlotPanel* panel_host(SciQLopMultiPlotPanel* panel, const char* what)
{
    if (panel == nullptr)
        throw std::invalid_argument(std::string(what) + ": panel cannot be None");
    return SciQLopPlots::on_owner_thread(panel, what);
}

/*!
 * The per-plot parts of a panel-wide object: one per SciQLopPlot of the panel (projection and
 * widget plots get none), built when the plot joins and deleted when it leaves. A plot deleted
 * directly takes its part with it, which leaves a null QPointer here.
 *
 * Every loop runs over a snapshot, so a slot that deletes a part, or the owner, is safe. While
 * write() runs, writing() is true: the owner ignores the parts' echoes of its own change, so one
 * change emits once.
 */
template <typename Part>
class PanelParts
{
    QObject* m_owner;
    std::function<Part*(SciQLopPlot*)> m_make;
    QList<QPointer<SciQLopPlotInterface>> m_plots;
    QList<QPointer<Part>> m_parts;
    bool m_writing = false;

    void update(const QList<QPointer<SciQLopPlotInterface>>& plots)
    {
        m_parts.removeIf([](const QPointer<Part>& part) { return part.isNull(); });
        for (const auto& plot : plots)
        {
            if (!plot || m_plots.contains(plot))
                continue;
            m_plots.append(plot);
            if (auto* scp = qobject_cast<SciQLopPlot*>(plot.data()))
                add(scp);
        }
        const auto known = m_plots;
        for (const auto& plot : known)
        {
            if (plots.contains(plot))
                continue;
            m_plots.removeOne(plot);
            remove(plot.data());
        }
    }

    void add(SciQLopPlot* plot)
    {
        auto* part = m_make(plot);
        SciQLopPlots::mark_panel_part(part);
        m_parts.append(part);
    }

    void remove(SciQLopPlotInterface* plot)
    {
        if (plot == nullptr)
            return;
        const auto parts = m_parts;
        for (const auto& part : parts)
        {
            if (part && part->parent() == plot)
            {
                m_parts.removeAll(part);
                delete part.data();
            }
        }
    }

public:
    PanelParts(QObject* owner, std::function<Part*(SciQLopPlot*)> make)
            : m_owner(owner), m_make(std::move(make))
    {
    }

    // Deletion needs no writing() guard, unlike write(): a dying part emits none of the signals
    // its owner follows (only QObject::destroyed, which nobody here connects), so nothing re-enters.
    ~PanelParts()
    {
        for_each([](Part* part) { delete part; });
    }

    PanelParts(const PanelParts&) = delete;
    PanelParts& operator=(const PanelParts&) = delete;

    //! Builds a part for each plot \a panel has, then follows its plot list.
    void follow(SciQLopMultiPlotPanel* panel)
    {
        QObject::connect(panel, &SciQLopPlotPanelInterface::plot_list_changed, m_owner,
                         [this](const QList<QPointer<SciQLopPlotInterface>>& plots)
                         { update(plots); });
        update(panel->plots());
    }

    [[nodiscard]] bool writing() const noexcept { return m_writing; }

    [[nodiscard]] Part* first() const
    {
        for (const auto& part : m_parts)
            if (part)
                return part.data();
        return nullptr;
    }

    template <typename F>
    void for_each(F&& f) const
    {
        const auto parts = m_parts;
        for (const auto& part : parts)
            if (part)
                f(part.data());
    }

    //! for_each with writing() raised. False when a slot deleted the owner: touch nothing then.
    template <typename F>
    [[nodiscard]] bool write(F&& f)
    {
        const QPointer<QObject> alive(m_owner);
        const bool was = std::exchange(m_writing, true);
        for_each(std::forward<F>(f));
        if (!alive)
            return false;
        m_writing = was;
        return true;
    }
};
} // namespace impl

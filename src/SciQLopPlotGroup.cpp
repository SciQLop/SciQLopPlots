/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#include "SciQLopPlots/SciQLopPlotGroup.hpp"
#include "SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp"
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include <algorithm>
#include <qcp.h>
#include <stdexcept>

namespace
{
// A plot's graph hangs off its internal canvas; a projection-level curve off the projection.
SciQLopPlotInterface* owner_of(const SciQLopPlottableInterface* member)
{
    QObject* parent = member->parent();
    if (auto* canvas = qobject_cast<_impl::SciQLopPlot*>(parent))
        parent = canvas->parent();
    return qobject_cast<SciQLopPlotInterface*>(parent);
}

bool is_projection_piece(const SciQLopNDProjectionPlot* projection,
                         const SciQLopPlottableInterface* member)
{
    const auto curves = projection->plottables();
    return std::ranges::any_of(curves,
                               [member](SciQLopPlottableInterface* p)
                               {
                                   auto* c = qobject_cast<SciQLopNDProjectionCurves*>(p);
                                   return c && c->is_pane_curve(member);
                               });
}
}

SciQLopPlotGroup::SciQLopPlotGroup(const QString& name, SciQLopPlotInterface* plot)
        : SciQLopDrawnObject(plot), m_plot(plot)
{
    set_name(name);
}

SciQLopPlotGroup::~SciQLopPlotGroup()
{
    for (const auto& group : m_canvas_groups)
        if (group)
            (void)group->parentPlot()->removePlottableGroup(group);
}

void SciQLopPlotGroup::_named(const QString& name)
{
    for (const auto& group : m_canvas_groups)
        if (group)
        {
            group->setName(name);
            group->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
}

QList<SciQLopPlottableInterface*> SciQLopPlotGroup::members() const
{
    QList<SciQLopPlottableInterface*> live;
    for (const auto& member : m_members)
        if (member)
            live.append(member);
    return live;
}

void SciQLopPlotGroup::check_member(SciQLopPlottableInterface* member) const
{
    if (!member)
        throw std::invalid_argument("Group: a member can't be None");
    auto* owner = owner_of(member);
    auto* projection = qobject_cast<SciQLopNDProjectionPlot*>(m_plot);
    const bool in_a_pane = projection && owner && owner->parentWidget() == projection;
    if (owner != m_plot && !in_a_pane)
        throw std::invalid_argument("Group: '" + member->name().toStdString()
                                    + "' belongs to another plot");
    if (in_a_pane && is_projection_piece(projection, member))
        throw std::invalid_argument("Group: '" + member->name().toStdString()
                                    + "' is one pane of a projection curve; group the "
                                      "projection curve instead");
}

void SciQLopPlotGroup::add(SciQLopPlottableInterface* member)
{
    SciQLopPlots::require_owner_thread(this, "SciQLopPlotGroup::add");
    check_member(member);
    if (members().contains(member))
        return;
    // Every signal waits until the move is done, and is guarded: a members_changed slot may
    // delete a group, this one included.
    QList<QPointer<SciQLopPlotGroup>> left;
    for (auto* other : m_plot->findChildren<SciQLopPlotGroup*>(Qt::FindDirectChildrenOnly))
        if (other != this && other->forget(member))
            left.append(other);
    m_members.append(member);
    connect(member, &QObject::destroyed, this, &SciQLopPlotGroup::forget_destroyed_members);
    connect(member, &SciQLopDrawnObject::visible_changed, this,
            [this] { _notify_visible(_visible()); });
    _seed_parts();
    if (auto* graph = qobject_cast<SciQLopGraphInterface*>(member))
        connect(graph, &SciQLopGraphInterface::component_list_changed, this,
                [this, member] { sync_canvas_groups(member); });
    sync_canvas_groups(member); // NeoQCP moves its plottables out of their old canvas groups

    QPointer<SciQLopPlotGroup> self = this;
    for (const auto& other : left)
        if (other)
            Q_EMIT other->members_changed();
    if (self)
        Q_EMIT members_changed();
}

bool SciQLopPlotGroup::forget(SciQLopPlottableInterface* member)
{
    if (!members().contains(member))
        return false;
    disconnect(member, nullptr, this, nullptr);
    m_members.removeAll(member);
    _seed_parts();
    return true;
}

void SciQLopPlotGroup::remove(SciQLopPlottableInterface* member)
{
    SCIQLOP_ON_OWNER_THREAD_WITH(member, remove(member));
    if (!member || !forget(member))
        return;
    for (auto* p : qcp_plottables_of(member))
        if (auto* group = p->group(); group && m_canvas_groups.contains(group))
        {
            group->removeMember(p);
            p->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
    Q_EMIT members_changed();
}

QCPPlottableGroup* SciQLopPlotGroup::canvas_group(QCustomPlot* canvas)
{
    for (const auto& group : m_canvas_groups)
        if (group && group->parentPlot() == canvas)
            return group;
    auto* group = canvas->addPlottableGroup(objectName());
    m_canvas_groups.append(group);
    // Toggling the members, not the per-pane groups, keeps every pane in step and fires each
    // member's visible_changed (QCPLayerable::setVisible alone would leave the inspector stale).
    connect(group, &QCPPlottableGroup::doubleClicked, this, [this] { set_visible(!visible()); });
    return group;
}

void SciQLopPlotGroup::sync_canvas_groups(SciQLopPlottableInterface* member)
{
    for (auto* p : qcp_plottables_of(member))
        if (auto* canvas = p->parentPlot())
            if (auto* group = canvas_group(canvas); p->group() != group)
            {
                group->addMember(p);
                canvas->replot(QCustomPlot::rpQueuedReplot);
            }
}

void SciQLopPlotGroup::forget_destroyed_members()
{
    // destroyed() fires after Qt has nulled the QPointer to the dying member.
    const bool forgot = m_members.removeIf([](const auto& m) { return m.isNull(); }) > 0;
    _seed_parts();
    if (forgot)
        Q_EMIT members_changed();
}

bool SciQLopPlotGroup::_visible() const
{
    const auto live = members();
    return std::ranges::any_of(live, [](SciQLopPlottableInterface* m) { return m->visible(); });
}

void SciQLopPlotGroup::_set_visible(bool visible)
{
    // A member's visible_changed slot may remove another member: iterate guarded pointers.
    const auto snapshot = m_members;
    for (const auto& member : snapshot)
        if (member)
            member->set_visible(visible);
}

void SciQLopPlotGroup::_seed_parts()
{
    _seed_visible(_visible());
}

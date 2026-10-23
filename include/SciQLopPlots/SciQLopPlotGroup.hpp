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
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include "SciQLopPlots/SciQLopDrawnObject.hpp"

class SciQLopPlotInterface;
class SciQLopPlottableInterface;
class QCPPlottableGroup;
class QCustomPlot;

/*! Plottables of one plot shown as one legend row, like a scene boundary's centre, min and
 *  max curves. On a projection plot the members can be in any pane: each pane holding some gets
 *  its own row, and a shared legend shows only the first pane's.
 *
 *  The group holds its members, it doesn't own them. Create it with SciQLopPlotInterface::group.
 */
class SciQLopPlotGroup : public SciQLopDrawnObject
{
    Q_OBJECT

    SciQLopPlotInterface* m_plot;
    QList<QPointer<SciQLopPlottableInterface>> m_members;
    QList<QPointer<QCPPlottableGroup>> m_canvas_groups;

    void check_member(SciQLopPlottableInterface* member) const;
    void sync_canvas_groups(SciQLopPlottableInterface* member);
    QCPPlottableGroup* canvas_group(QCustomPlot* canvas);
    void forget_destroyed_members();
    bool forget(SciQLopPlottableInterface* member);
    void _seed_parts();

    // Made by SciQLopPlotInterface::group, which checks the thread and the members.
    SciQLopPlotGroup(const QString& name, SciQLopPlotInterface* plot);
    friend class SciQLopPlotInterface;

public:
    ~SciQLopPlotGroup() override;

    QList<SciQLopPlottableInterface*> members() const;
    //! Raises ValueError for a plottable of another plot, or a pane piece of a projection curve.
    void add(SciQLopPlottableInterface* member);
    void remove(SciQLopPlottableInterface* member);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void members_changed();

protected:
    bool _always_apply() const override { return true; }
    //! True while any member is visible.
    bool _visible() const override;
    void _set_visible(bool visible) override;
    void _named(const QString& name) override;
};

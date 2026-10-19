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
class SciQLopPlotGroup : public QObject
{
    Q_OBJECT

    SciQLopPlotInterface* m_plot;
    QString m_name;
    QList<QPointer<SciQLopPlottableInterface>> m_members;
    QList<QPointer<QCPPlottableGroup>> m_canvas_groups;

    void check_member(SciQLopPlottableInterface* member) const;
    void sync_canvas_groups(SciQLopPlottableInterface* member);
    QCPPlottableGroup* canvas_group(QCustomPlot* canvas);
    void forget_destroyed_members();

public:
    SciQLopPlotGroup(const QString& name, SciQLopPlotInterface* plot);
    ~SciQLopPlotGroup() override;

    QString name() const { return m_name; }
    void set_name(const QString& name);

    QList<SciQLopPlottableInterface*> members() const;
    //! Raises ValueError for a plottable of another plot, or a pane piece of a projection curve.
    void add(SciQLopPlottableInterface* member);
    void remove(SciQLopPlottableInterface* member);

    //! True while any member is visible.
    bool visible() const;
    void set_visible(bool visible);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void members_changed();
};

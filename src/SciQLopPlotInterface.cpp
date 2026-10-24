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

#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/Items/SciQLopPlotItem.hpp"
#include "SciQLopPlots/Items/SciQLopStraightLines.hpp"
#include "SciQLopPlots/SciQLopTheme.hpp"
#include <QColor>
#include <stdexcept>

void SciQLopPlotInterface::apply_selection_style()
{
    if (m_selected)
    {
        QColor border_color = Qt::black;
        if (auto* t = this->theme(); t)
        {
            const QColor themed = t->selection();
            if (themed.isValid())
                border_color = themed;
        }
        this->setStyleSheet(
            QStringLiteral("border: 2px solid %1;border-style: dashed;").arg(border_color.name()));
    }
    else
    {
        this->setStyleSheet(QStringLiteral("border: 0px;"));
    }
}

void SciQLopPlotInterface::set_selected(bool selected) noexcept
{
    if (m_selected != selected)
    {
        m_selected = selected;
        apply_selection_style();
        Q_EMIT selection_changed(selected);
    }
}

SciQLopPlotGroup* SciQLopPlotInterface::group(const QString& name,
                                              const QList<SciQLopPlottableInterface*>& members)
{
    SciQLopPlots::require_owner_thread(this, "SciQLopPlotInterface::group");
    std::unique_ptr<SciQLopPlotGroup> group { new SciQLopPlotGroup(name, this) };
    for (auto* member : members)
        group->add(member);
    return group.release();
}

QList<SciQLopPlotGroup*> SciQLopPlotInterface::groups() const
{
    return findChildren<SciQLopPlotGroup*>(Qt::FindDirectChildrenOnly);
}

//! Deprecated: remove(). A group of another plot raises, a null one does nothing.
void SciQLopPlotInterface::remove_group(SciQLopPlotGroup* group)
{
    if (group)
        remove(group);
}

namespace
{
SciQLopPlotInterface* owning_plot(const QObject* object)
{
    for (auto* p = object->parent(); p; p = p->parent())
        if (auto* plot = qobject_cast<SciQLopPlotInterface*>(p))
            return plot;
    return nullptr;
}
}

void SciQLopPlotInterface::check_removable(const QObject* x) const
{
    if (x == nullptr)
        throw std::invalid_argument("remove: got None");
    const std::string what = x->metaObject()->className();
    if (SciQLopPlots::is_panel_part(x))
        throw std::invalid_argument("remove: this " + what
                                    + " is part of a panel-wide object; remove that object from "
                                      "the panel (panel.remove)");
    if (auto* owner = owning_plot(x); owner != this)
        throw std::invalid_argument(
            owner ? "remove: this " + what + " belongs to another plot ('"
                    + owner->objectName().toStdString() + "')"
                  : "remove: this " + what + " does not belong to any plot");
}

// Checked before queuing: off-thread, a throw from the queued call would escape the event loop.
void SciQLopPlotInterface::remove(SciQLopPlottableInterface* plottable)
{
    check_removable(plottable);
    SCIQLOP_ON_OWNER_THREAD_WITH(plottable, remove(plottable));
    delete plottable;
}

void SciQLopPlotInterface::remove(SciQLopPlotGroup* group)
{
    check_removable(group);
    SCIQLOP_ON_OWNER_THREAD_WITH(group, remove(group));
    delete group;
}

void SciQLopPlotInterface::remove(SciQLopItemInterface* item)
{
    check_removable(item);
    SCIQLOP_ON_OWNER_THREAD_WITH(item, remove(item));
    delete item;
}

void SciQLopPlotInterface::remove(SciQLopStraightLine* line)
{
    check_removable(line);
    SCIQLOP_ON_OWNER_THREAD_WITH(line, remove(line));
    delete line;
}

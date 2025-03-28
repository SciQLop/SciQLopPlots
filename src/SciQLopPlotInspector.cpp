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
#include "SciQLopPlots/Inspector/Inspectors/SciQLopPlotInspector.hpp"
#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"

REGISTER_INSPECTOR(SciQLopPlotInspector)

QList<QObject*> SciQLopPlotInspector::children(QObject* obj)
{
    QList<QObject*> children;
    if (auto plot = _plot(obj); plot)
    {
        children.append(plot->x_axis());
        children.append(plot->y_axis());
        if (plot->has_colormap())
        {
            children.append(plot->z_axis());
            children.append(plot->y2_axis());
        }
        for (auto c : plot->plottables())
        {
            children.append(c);
        }
    }
    return children;
}

QObject* SciQLopPlotInspector::child(const QString& name, QObject* obj)
{
    if (auto plot = _plot(obj); plot)
    {
        if (auto axis = plot->axis(name))
            return axis;
        return plot->plottable(name);
    }
    return nullptr;
}

void SciQLopPlotInspector::connect_node(PlotsModelNode* node, QObject* const obj)
{
    InspectorBase::connect_node(node, obj);
    if (auto plot = _plot(obj); plot)
    {
        connect(plot, &SciQLopPlot::graph_list_changed, node, &PlotsModelNode::childrenChanged);
    }
}

void SciQLopPlotInspector::set_selected(QObject* obj, bool selected)
{
    if (auto plot = _plot(obj); plot)
        plot->set_selected(selected);
}

Qt::ItemFlags SciQLopPlotInspector::flags()
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

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
#include "SciQLopPlots/Inspector/Inspectors/SciQLopMultiPlotPanelInspector.hpp"
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"

REGISTER_INSPECTOR(SciQLopMultiPlotPanelInspector)

QList<QObject*> SciQLopMultiPlotPanelInspector::children(QObject* obj)
{
    QList<QObject*> children;
    if (auto panel = _panel(obj); panel)
    {
        for (auto w : panel->child_widgets())
        {
            children.append(w);
        }
    }
    return children;
}

QObject* SciQLopMultiPlotPanelInspector::child(const QString& name, QObject* obj)
{
    if (auto panel = _panel(obj); panel)
    {
        for (auto plot : panel->plots())
        {
            if (plot->objectName() == name)
                return plot;
        }
    }
    return nullptr;
}

void SciQLopMultiPlotPanelInspector::connect_node(PlotsModelNode* node, QObject* const obj)
{
    auto panel = _panel(obj);
    InspectorBase::connect_node(node, obj);
    connect(panel, &SciQLopMultiPlotPanel::plot_added, node,&PlotsModelNode::childrenChanged);
    connect(panel, &SciQLopMultiPlotPanel::plot_removed, node,&PlotsModelNode::childrenChanged);
    connect(panel, &SciQLopMultiPlotPanel::panel_added, node, &PlotsModelNode::childrenChanged);
    connect(panel, &SciQLopMultiPlotPanel::panel_removed, node, &PlotsModelNode::childrenChanged);
}

void SciQLopMultiPlotPanelInspector::set_selected(QObject* obj, bool selected)
{
    if (auto panel = _panel(obj); panel)
        panel->setSelected(selected);
}

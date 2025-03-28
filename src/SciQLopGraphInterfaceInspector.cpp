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
#include "SciQLopPlots/Inspector/Inspectors/SciQLopGraphInterfaceInspector.hpp"
#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Inspector/Model/Node.hpp"

REGISTER_INSPECTOR(SciQLopGraphInterfaceInspector)

QList<QObject*> SciQLopGraphInterfaceInspector::children(QObject* obj)
{
    QList<QObject*> children;
    if (auto graph = _graph(obj); graph)
    {
        for (auto component : graph->components())
        {
            children.append(component);
        }
    }
    return children;
}

QObject* SciQLopGraphInterfaceInspector::child(const QString& name, QObject* obj)
{
    if (auto graph = _graph(obj); graph)
    {
        return graph->component(name);
    }
    return nullptr;
}

void SciQLopGraphInterfaceInspector::connect_node(PlotsModelNode* node, QObject* const obj)
{
    InspectorBase::connect_node(node, obj);
    if (auto graph = _graph(obj); graph)
    {
        connect(graph, &SciQLopGraphInterface::selection_changed, node,
                &PlotsModelNode::set_selected);
        connect(graph, &SciQLopGraphInterface::component_list_changed, node,
                &PlotsModelNode::childrenChanged);
    }
}

void SciQLopGraphInterfaceInspector::set_selected(QObject* obj, bool selected)
{
    if (auto graph = _graph(obj); graph != nullptr && selected != graph->selected())
    {
        graph->set_selected(selected);
    }
}

bool SciQLopGraphInterfaceInspector::selected(const QObject* obj)
{
    if (auto graph = _graph(obj); graph)
    {
        return graph->selected();
    }
    return false;
}

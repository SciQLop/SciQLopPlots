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
#include "SciQLopPlots/Inspector/Inspectors/SciQLopGraphComponentInspector.hpp"
#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Inspector/Model/Node.hpp"

REGISTER_INSPECTOR(SciQLopGraphComponentInspector)

QList<QObject*> SciQLopGraphComponentInspector::children(QObject* obj)
{
    return {};
}

QObject* SciQLopGraphComponentInspector::child(const QString& name, QObject* obj)
{
    return nullptr;
}

void SciQLopGraphComponentInspector::connect_node(PlotsModelNode* node, QObject* const obj)
{
    InspectorBase::connect_node(node, obj);
    if (auto component = _component(obj); component)
    {
        connect(component, &SciQLopGraphComponentInterface::selection_changed, node,
                &PlotsModelNode::set_selected);
    }
}

void SciQLopGraphComponentInspector::set_selected(QObject* obj, bool selected)
{
    if (auto component = _component(obj); component)
    {
        component->set_selected(selected);
    }
}

bool SciQLopGraphComponentInspector::selected(const QObject* obj)
{
    if (auto component = _component(obj); component)
    {
        return component->selected();
    }
    return false;
}

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
#include "SciQLopPlots/Inspector/Inspectors/SciQLopColorMapInspector.hpp"
#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"


REGISTER_INSPECTOR(SciQLopColorMapInspector);

QList<QObject*> SciQLopColorMapInspector::children(QObject* obj)
{
    Q_UNUSED(obj);
    return {};
}

QObject* SciQLopColorMapInspector::child(const QString& name, QObject* obj)
{
    Q_UNUSED(name);
    Q_UNUSED(obj);
    return nullptr;
}

void SciQLopColorMapInspector::connect_node(PlotsModelNode* node, QObject* const obj)
{
    InspectorBase::connect_node(node, obj);
    if (auto cmap = _colormap(obj); cmap)
    {
        connect(cmap, &SciQLopColorMap::selection_changed, node, &PlotsModelNode::set_selected);
    }
}

void SciQLopColorMapInspector::set_selected(QObject* obj, bool selected)
{
    auto colormap = _colormap(obj);
    if (colormap)
    {
        colormap->set_selected(selected);
    }
}

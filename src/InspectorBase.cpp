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
#include "SciQLopPlots/Inspector/InspectorBase.hpp"
#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include <QStringLiteral>

REGISTER_INSPECTOR(InspectorBase)

InspectorBase::InspectorBase(QObject* parent) { }

QList<QObject*> InspectorBase::children(QObject* obj)
{
    if (obj != nullptr)
        return obj->children();
    return {};
}

QObject* InspectorBase::child(const QString& name, QObject* obj)
{
    if (obj != nullptr)
        return obj->findChild<QObject*>(name);
    return nullptr;
}

QIcon InspectorBase::icon(const QObject* const obj)
{
    Q_UNUSED(obj);
    return QIcon();
}

QString InspectorBase::tooltip(const QObject* const obj)
{
    Q_UNUSED(obj);
    return QString();
}

void InspectorBase::connect_node(PlotsModelNode* node, QObject* const obj)
{
    connect(obj, &QObject::destroyed, node, &PlotsModelNode::objectDestroyed);
    connect(obj, &QObject::objectNameChanged, node, &PlotsModelNode::setName);
}

void InspectorBase::set_selected(QObject* obj, bool selected)
{
    Q_UNUSED(obj);
    Q_UNUSED(selected);
}

bool InspectorBase::selected(const QObject* obj)
{
    Q_UNUSED(obj);
    return false;
}

bool InspectorBase::deletable(const QObject* obj)
{
    Q_UNUSED(obj);
    return true;
}

Qt::ItemFlags InspectorBase::flags()
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

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
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include "SciQLopPlots/Inspector/InspectorBase.hpp"
#include "SciQLopPlots/Inspector/Inspectors.hpp"


PlotsModelNode* PlotsModelNode::_root_node()
{
    PlotsModelNode* node = this;
    while (node->parent_node() != nullptr)
        node = node->parent_node();
    return node;
}

PlotsModelNode::PlotsModelNode(QObject* obj, QObject* parent) : QObject(parent), m_obj(obj)
{
    if (obj != nullptr)
    {
        setObjectName(obj->objectName());
        connect(this, &PlotsModelNode::objectNameChanged, this, &PlotsModelNode::nameChanged);
        connect(obj, &QObject::destroyed, this, &PlotsModelNode::objectDestroyed);
        m_inspector = Inspectors::inspector(obj);
        if (m_inspector != nullptr)
        {
            set_deletable(m_inspector->deletable(obj));
            m_inspector->connect_node(this, obj);
        }
    }
}

PlotsModelNode::~PlotsModelNode()
{
    while (!m_children.isEmpty())
    {
        delete m_children.takeFirst();
    }
    if (m_obj)
        m_obj->deleteLater();
}

PlotsModelNode* PlotsModelNode::insert_child(QObject* obj, int row)
{
    PlotsModelNode* node = new PlotsModelNode(obj, this);
    if (row == -1)
        m_children.append(node);
    else
        m_children.insert(row, node);
    return node;
}

PlotsModelNode* PlotsModelNode::child_node(const QString& name)
{
    for (auto child : m_children)
    {
        if (child->name() == name)
            return child;
    }
    return nullptr;
}

int PlotsModelNode::child_row(QObject* obj)
{
    for (int i = 0; i < children_count(); i++)
    {
        if (m_children[i]->holds(obj))
            return i;
    }
    return -1;
}

bool PlotsModelNode::remove_child(int row, bool destroy)
{
    if (row < 0 || row >= m_children.size())
        return false;
    auto child = m_children.at(row);
    m_children.removeAt(row);
    if (destroy)
    {
        delete child;
        emit childrenDestroyed(this, row);
    }
    return true;
}

QIcon PlotsModelNode::icon()
{
    if (m_inspector == nullptr || m_obj == nullptr)
        return QIcon();
    return m_inspector->icon(m_obj);
}

QString PlotsModelNode::tooltip()
{
    if (m_inspector == nullptr || m_obj == nullptr)
        return QString();
    return m_inspector->tooltip(m_obj);
}

void PlotsModelNode::setName(const QString& name)
{
    if (objectName() != name)
    {
        setObjectName(name);
    }
    if (m_obj && m_obj->objectName() != name)
        m_obj->setObjectName(name);
}

void PlotsModelNode::set_selected(bool selected)
{
    if (m_inspector && m_obj)
    {
        m_inspector->set_selected(m_obj.data(), selected);
        emit selectionChanged(selected);
    }
}

Qt::ItemFlags PlotsModelNode::flags()
{
    if (m_inspector)
        return m_inspector->flags();
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

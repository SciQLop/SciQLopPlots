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

void PlotsModelNode::_child_destroyed(PlotsModelNode* child)
{
    m_children.removeOne(child);
    emit childrenChanged(this);
}

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
        m_inspector = Inspectors::inspector(obj);
        if (m_inspector != nullptr)
        {
            m_inspector->connect_node(this, obj);
            for (auto child : m_inspector->children(obj))
            {
                insert_child(child);
            }
        }
    }
}

PlotsModelNode* PlotsModelNode::insert_child(QObject* obj, int row)
{
    PlotsModelNode* node = new PlotsModelNode(obj, this);
    if (row == -1)
        m_children.append(node);
    else
        m_children.insert(row, node);
    connect(node, &QObject::destroyed, this, [this, node]() { _child_destroyed(node); });
    connect(node, &PlotsModelNode::childrenChanged, _root_node(), &PlotsModelNode::childrenChanged);
    emit childrenChanged(this);
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

QIcon PlotsModelNode::icon()
{
    if (m_inspector == nullptr)
        return QIcon();
    return m_inspector->icon(m_obj);
}

QString PlotsModelNode::tooltip()
{
    if (m_inspector == nullptr)
        return QString();
    return m_inspector->tooltip(m_obj);
}

void PlotsModelNode::setName(const QString& name)
{
    if (objectName() != name)
    {
        setObjectName(name);
        emit nameChanged(name);
    }
}

void PlotsModelNode::update_children()
{
    if (m_inspector != nullptr)
    {
        auto new_children = m_inspector->children(m_obj);
        for (auto child : new_children)
        {
            if (!contains(child))
                insert_child(child);
        }
        for (auto child : m_children)
        {
            if (!new_children.contains(child->m_obj))
                delete child;
        }
    }
    emit childrenChanged(this);
}

void PlotsModelNode::set_selected(bool selected)
{
    if (m_inspector)
        m_inspector->set_selected(m_obj, selected);
}

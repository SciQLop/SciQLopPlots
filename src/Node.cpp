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
#include "SciQLopPlots/Inspector/Model/TypeDescriptor.hpp"

PlotsModelNode::PlotsModelNode(QObject* obj, const TypeDescriptor* desc, QObject* parent)
    : QObject(parent), m_obj(obj), m_descriptor(desc)
{
    if (obj)
        setObjectName(obj->objectName());
}

PlotsModelNode::~PlotsModelNode()
{
    while (!m_children.isEmpty())
    {
        auto child = m_children.takeFirst();
        child->disconnect_all();
        delete child;
    }
}

void PlotsModelNode::setName(const QString& name)
{
    if (objectName() != name)
        setObjectName(name);
    if (m_obj && m_obj->objectName() != name)
        m_obj->setObjectName(name);
}

PlotsModelNode* PlotsModelNode::child(int row) const
{
    return m_children.value(row, nullptr);
}

int PlotsModelNode::child_row(PlotsModelNode* child) const
{
    return m_children.indexOf(child);
}

int PlotsModelNode::child_row_by_object(QObject* obj) const
{
    for (int i = 0; i < m_children.size(); ++i)
    {
        if (m_children[i]->object() == obj)
            return i;
    }
    return -1;
}

int PlotsModelNode::children_count() const
{
    return m_children.size();
}

QList<PlotsModelNode*> PlotsModelNode::children_nodes() const
{
    return m_children;
}

PlotsModelNode* PlotsModelNode::parent_node() const
{
    return qobject_cast<PlotsModelNode*>(parent());
}

PlotsModelNode* PlotsModelNode::insert_child(
    QObject* obj, const TypeDescriptor* desc, int row)
{
    auto node = new PlotsModelNode(obj, desc, this);
    if (row < 0 || row >= m_children.size())
        m_children.append(node);
    else
        m_children.insert(row, node);
    return node;
}

bool PlotsModelNode::remove_child(int row)
{
    if (row < 0 || row >= m_children.size())
        return false;
    m_children.removeAt(row);
    return true;
}

void PlotsModelNode::add_connections(const QList<QMetaObject::Connection>& conns)
{
    m_connections.append(conns);
}

void PlotsModelNode::add_connection(QMetaObject::Connection conn)
{
    m_connections.append(std::move(conn));
}

void PlotsModelNode::disconnect_all()
{
    for (auto& conn : m_connections)
        QObject::disconnect(conn);
    m_connections.clear();
}

bool PlotsModelNode::deletable() const
{
    return m_descriptor ? m_descriptor->deletable : true;
}

Qt::ItemFlags PlotsModelNode::flags() const
{
    return m_descriptor ? m_descriptor->flags
                        : (Qt::ItemIsEnabled | Qt::ItemIsSelectable);
}

QIcon PlotsModelNode::icon() const
{
    if (m_descriptor && m_descriptor->icon && m_obj)
        return m_descriptor->icon(m_obj);
    return {};
}

QString PlotsModelNode::tooltip() const
{
    if (m_descriptor && m_descriptor->tooltip && m_obj)
        return m_descriptor->tooltip(m_obj);
    return {};
}

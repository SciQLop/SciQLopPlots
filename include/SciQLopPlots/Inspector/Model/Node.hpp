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
#pragma once
#include <QIcon>
#include <QObject>
#include <QUuid>

class InspectorBase;

class PlotsModelNode : public QObject
{
    Q_OBJECT
    QObject* m_obj;
    InspectorBase* m_inspector;
    QList<PlotsModelNode*> m_children;

    void _child_destroyed(PlotsModelNode* child);
    PlotsModelNode* _root_node();

public:
    PlotsModelNode(QObject* obj, QObject* parent = nullptr);
    ~PlotsModelNode() = default;

    PlotsModelNode* insert_child(QObject* obj, int row = -1);

    inline QString name() { return objectName(); }

    PlotsModelNode* child_node(const QString& name);

    inline QList<PlotsModelNode*> children_nodes() { return m_children; }

    PlotsModelNode* parent_node() { return qobject_cast<PlotsModelNode*>(parent()); }

    inline PlotsModelNode* child(int row) { return m_children.value(row, nullptr); }

    inline int child_row(PlotsModelNode* child) { return m_children.indexOf(child); }

    inline int children_count() { return m_children.size(); }

    QIcon icon();
    QString tooltip();

    Q_SLOT void setName(const QString& name);
    Q_SLOT void update_children();

    inline bool holds(QObject* obj)
    {
        return m_obj == obj && m_obj->objectName() == obj->objectName();
    }

    inline bool contains(QObject* obj)
    {
        for (auto child : m_children)
        {
            if (child->holds(obj))
                return true;
        }
        return false;
    }

    void set_selected(bool selected);

#ifndef BINDINGS_H
    Q_SIGNAL void nameChanged(const QString& name);
    Q_SIGNAL void childrenChanged(PlotsModelNode* node);
#endif
};

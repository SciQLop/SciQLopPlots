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
#include <QPointer>
#include <QUuid>

class InspectorBase;

class PlotsModelNode : public QObject
{
    Q_OBJECT
    QPointer<QObject> m_obj;
    InspectorBase* m_inspector;
    QList<PlotsModelNode*> m_children;

    PlotsModelNode* _root_node();
    bool _deletable = true;

public:
    PlotsModelNode(QObject* obj, QObject* parent = nullptr);
    ~PlotsModelNode();

    PlotsModelNode* insert_child(QObject* obj, int row = -1);

    inline QString name() { return objectName(); }

    PlotsModelNode* child_node(const QString& name);

    inline QList<PlotsModelNode*> children_nodes() { return m_children; }

    PlotsModelNode* parent_node() { return qobject_cast<PlotsModelNode*>(parent()); }

    inline PlotsModelNode* child(int row) { return m_children.value(row, nullptr); }

    inline int child_row(PlotsModelNode* child) { return m_children.indexOf(child); }

    int child_row(QObject* obj);

    inline int children_count() { return m_children.size(); }

    bool remove_child(int row, bool destroy = true);

    QIcon icon();
    QString tooltip();

    Q_SLOT void setName(const QString& name);

    inline bool holds(QObject* obj)
    {
        if (m_obj)
            return m_obj == obj && m_obj->objectName() == obj->objectName();
        return false;
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

    inline void set_deletable(bool deletable) { _deletable = deletable; }

    inline bool deletable() { return _deletable; }

    inline QObject* object() { return m_obj; }

    inline InspectorBase* inspector() { return m_inspector; }

    Qt::ItemFlags flags();


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void nameChanged();
    Q_SIGNAL void childrenChanged();
    Q_SIGNAL void childrenDestroyed(PlotsModelNode* parent, int row);
    Q_SIGNAL void selectionChanged(bool selected);
    Q_SIGNAL void objectDestroyed();
};

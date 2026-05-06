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
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

struct TypeDescriptor;

class PlotsModelNode : public QObject
{
    Q_OBJECT
    QPointer<QObject> m_obj;
    const TypeDescriptor* m_descriptor;
    QList<PlotsModelNode*> m_children;
    QList<QMetaObject::Connection> m_connections;

public:
    PlotsModelNode(QObject* obj, const TypeDescriptor* desc, QObject* parent = nullptr);
    ~PlotsModelNode();

    inline QString name() const { return objectName(); }
    void setName(const QString& name);

    PlotsModelNode* child(int row) const;
    int child_row(PlotsModelNode* child) const;
    int child_row_by_object(QObject* obj) const;
    int children_count() const;
    QList<PlotsModelNode*> children_nodes() const;
    PlotsModelNode* parent_node() const;

    PlotsModelNode* insert_child(QObject* obj, const TypeDescriptor* desc, int row = -1);
    void insert_child_node(PlotsModelNode* node, int row);
    bool remove_child(int row);

    void add_connections(const QList<QMetaObject::Connection>& conns);
    void add_connection(QMetaObject::Connection conn);
    void disconnect_all();

    inline QObject* object() const { return m_obj; }
    inline const TypeDescriptor* descriptor() const { return m_descriptor; }

    bool deletable() const;
    Qt::ItemFlags flags() const;
    QIcon icon() const;
    QString tooltip() const;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void nameChanged();
};

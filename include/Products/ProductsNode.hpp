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
#include <QMap>
#include <QObject>
#include <QVariant>

enum class ProductsModelNodeType
{
    ROOT,
    PRODUCT,
    FOLDER
};

class ProductsModelNode : public QObject
{
    Q_OBJECT
    QList<ProductsModelNode*> m_children;
    ProductsModelNodeType m_node_type;

    ProductsModelNode* _root_node();

    QMap<QString, QVariant> m_metadata;

    QIcon m_icon;
    QString m_tooltip;

public:
    ProductsModelNode(const QString& name, const QMap<QString, QVariant>& metadata,
                      ProductsModelNodeType m_node_type, QObject* parent = nullptr);
    virtual ~ProductsModelNode() = default;

    ProductsModelNode* parent_node() { return qobject_cast<ProductsModelNode*>(parent()); }

    inline ProductsModelNode* child(int row) { return m_children.value(row, nullptr); }

    inline ProductsModelNode* child(const QString& name)
    {
        for (auto child : m_children)
        {
            if (child->objectName() == name)
                return child;
        }
        return nullptr;
    }

    inline int child_row(ProductsModelNode* child) { return m_children.indexOf(child); }

    inline int children_count() { return m_children.size(); }

    inline QList<ProductsModelNode*> children_nodes() { return m_children; }

    void add_child(ProductsModelNode* child);

    QStringList path();

    void set_icon(const QIcon& icon);
    QIcon icon();
    void set_tooltip(const QString& tooltip);
    QString tooltip();

    QMap<QString, QVariant> metadata() { return m_metadata; }

    inline ProductsModelNodeType node_type() const { return m_node_type; }

    inline QString name() { return objectName(); }
};

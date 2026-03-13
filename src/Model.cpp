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
#include "SciQLopPlots/Inspector/Model/Model.hpp"
#include "SciQLopPlots/Inspector/Model/TypeDescriptor.hpp"
#include <QMimeData>
#include <QPointer>
#include <qapplicationstatic.h>

PlotsModel::PlotsModel(QObject* parent) : QAbstractItemModel(parent)
{
    m_rootNode = new PlotsModelNode(nullptr, nullptr, this);
    m_rootNode->setObjectName(QStringLiteral("Root Node"));
}

QModelIndex PlotsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    auto parentNode = parent.isValid()
        ? static_cast<PlotsModelNode*>(parent.internalPointer())
        : m_rootNode;
    if (auto child = parentNode->child(row))
        return createIndex(row, column, child);
    return {};
}

QModelIndex PlotsModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};
    auto child = static_cast<PlotsModelNode*>(index.internalPointer());
    auto parentNode = child->parent_node();
    if (!parentNode || parentNode == m_rootNode)
        return {};
    auto grandparent = parentNode->parent_node();
    int row = grandparent ? grandparent->child_row(parentNode) : 0;
    return createIndex(row, 0, parentNode);
}

int PlotsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    auto parentNode = parent.isValid()
        ? static_cast<PlotsModelNode*>(parent.internalPointer())
        : m_rootNode;
    return parentNode->children_count();
}

int PlotsModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant PlotsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    auto node = static_cast<PlotsModelNode*>(index.internalPointer());
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::UserRole:
            return node->name();
        case Qt::DecorationRole:
            return node->icon();
        case Qt::ToolTipRole:
            return node->tooltip();
        default:
            return {};
    }
}

Qt::ItemFlags PlotsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    if (auto node = static_cast<PlotsModelNode*>(index.internalPointer()))
        return node->flags();
    return QAbstractItemModel::flags(index);
}

bool PlotsModel::removeRows(int row, int count, const QModelIndex& parent)
{
    auto parentNode = parent.isValid()
        ? static_cast<PlotsModelNode*>(parent.internalPointer())
        : m_rootNode;
    if (!parentNode || row < 0 || row + count > parentNode->children_count())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = row + count - 1; i >= row; --i)
    {
        auto child = parentNode->child(i);
        if (!child)
            continue;
        child->disconnect_all();
        QObject* obj = child->object();
        bool should_delete_obj = obj && child->deletable();
        parentNode->remove_child(i);
        emit node_removed(obj);
        delete child;
        if (should_delete_obj)
            obj->deleteLater();
    }
    endRemoveRows();
    return true;
}

void PlotsModel::addNode(PlotsModelNode* parent, QObject* obj)
{
    if (!obj)
        return;
    auto desc = TypeRegistry::instance().descriptor(obj);
    auto parentIndex = make_index(parent);
    int row = parent->children_count();

    beginInsertRows(parentIndex, row, row);
    auto node = parent->insert_child(obj, desc);
    endInsertRows();

    QPointer<PlotsModelNode> guardedNode = node;
    QPointer<PlotsModelNode> guardedParent = parent;

    node->add_connection(
        connect(obj, &QObject::destroyed, this,
            [this, guardedNode, guardedParent]()
            {
                if (!guardedNode || !guardedParent)
                    return;
                int r = guardedParent->child_row(guardedNode);
                if (r >= 0)
                    removeRows(r, 1, make_index(guardedParent));
            }));

    node->add_connection(
        connect(obj, &QObject::objectNameChanged, this,
            [this, guardedNode](const QString& name)
            {
                if (!guardedNode)
                    return;
                guardedNode->setName(name);
                auto idx = make_index(guardedNode);
                if (idx.isValid())
                    emit dataChanged(idx, idx);
            }));

    if (desc && desc->connect_children)
    {
        auto add_child = [this, guardedNode](QObject* child)
        {
            if (guardedNode)
                addNode(guardedNode, child);
        };
        auto remove_child = [this, guardedNode](QObject* child)
        {
            if (guardedNode)
                removeChildByObject(guardedNode, child);
        };
        node->add_connections(desc->connect_children(obj, add_child, remove_child));
    }

    if (desc && desc->children)
    {
        for (auto child : desc->children(obj))
            addNode(node, child);
    }
}

void PlotsModel::removeChildByObject(PlotsModelNode* parent, QObject* obj)
{
    if (!parent || !obj)
        return;
    int row = parent->child_row_by_object(obj);
    if (row >= 0)
        removeRows(row, 1, make_index(parent));
}

void PlotsModel::set_selected(const QList<QModelIndex>& indexes, bool selected)
{
    for (const auto& index : indexes)
    {
        auto node = static_cast<PlotsModelNode*>(index.internalPointer());
        if (node && node->descriptor() && node->descriptor()->set_selected && node->object())
        {
            node->descriptor()->set_selected(node->object(), selected);
            emit item_selection_changed(index, selected);
        }
    }
}

void PlotsModel::addTopLevelNode(QObject* obj)
{
    addNode(m_rootNode, obj);
    emit top_level_nodes_list_changed();
}

Q_APPLICATION_STATIC(PlotsModel, _plots_model);

PlotsModel* PlotsModel::instance()
{
    return _plots_model();
}

QObject* PlotsModel::object(const QModelIndex& index)
{
    if (auto node = static_cast<PlotsModelNode*>(index.internalPointer()))
        return node->object();
    return nullptr;
}

QMimeData* PlotsModel::mimeData(const QModelIndexList&) const
{
    return new QMimeData();
}

QModelIndex PlotsModel::make_index(PlotsModelNode* node) const
{
    if (!node || node == m_rootNode)
        return {};
    auto parentNode = node->parent_node();
    int row = parentNode ? parentNode->child_row(node) : m_rootNode->child_row(node);
    if (row < 0)
        return {};
    return createIndex(row, 0, node);
}

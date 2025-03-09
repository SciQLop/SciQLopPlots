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
#include <QMimeData>
#include <qapplicationstatic.h>

void PlotsModel::node_changed(PlotsModelNode* node)
{
    auto index = make_index(node);
    if (index.isValid())
        emit dataChanged(index, index);
}

void PlotsModel::node_selection_changed(PlotsModelNode* node, bool selected)
{
    auto index = make_index(node);
    if (index.isValid())
        emit item_selection_changed(index, selected);
}

void PlotsModel::children_destroyed(PlotsModelNode* parent, int index)
{
    beginRemoveRows(make_index(parent), index, index);
    endRemoveRows();
}

PlotsModel::PlotsModel(QObject* parent) : QAbstractItemModel(parent)
{
    m_rootNode = new PlotsModelNode(this);
    connect(m_rootNode, &PlotsModelNode::childrenChanged, this, &PlotsModel::node_changed,
            Qt::DirectConnection);
    connect(m_rootNode, &PlotsModelNode::childrenDestroyed, this, &PlotsModel::children_destroyed,
            Qt::DirectConnection);
    connect(m_rootNode, &PlotsModelNode::nameChanged, this, &PlotsModel::node_changed);
    connect(m_rootNode, &PlotsModelNode::selectionChanged, this,
            &PlotsModel::node_selection_changed);
}

QModelIndex PlotsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (hasIndex(row, column, parent))
    {
        PlotsModelNode* parentNode;
        if (!parent.isValid())
            parentNode = m_rootNode;
        else
            parentNode = static_cast<PlotsModelNode*>(parent.internalPointer());
        PlotsModelNode* childNode = parentNode->child(row);
        if (childNode)
            return createIndex(row, column, childNode);
    }
    return QModelIndex();
}

QModelIndex PlotsModel::parent(const QModelIndex& index) const
{
    if (index.isValid())
    {
        PlotsModelNode* childNode = static_cast<PlotsModelNode*>(index.internalPointer());
        PlotsModelNode* parentNode = childNode->parent_node();
        if (parentNode != m_rootNode)
        {
            if (parentNode != nullptr)
            {
                PlotsModelNode* grandparentNode = parentNode->parent_node();
                if (grandparentNode != nullptr)
                    return createIndex(grandparentNode->child_row(parentNode), 0, parentNode);
                return createIndex(0, 0, parentNode);
            }
        }
    }
    return QModelIndex();
}

int PlotsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    PlotsModelNode* parentNode;
    if (!parent.isValid())
        parentNode = m_rootNode;
    else
        parentNode = static_cast<PlotsModelNode*>(parent.internalPointer());
    return parentNode->children_count();
}

int PlotsModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant PlotsModel::data(const QModelIndex& index, int role) const
{
    if (index.isValid())
    {
        PlotsModelNode* node = static_cast<PlotsModelNode*>(index.internalPointer());
        switch (role)
        {
            case Qt::DisplayRole:
                return node->name();
            case Qt::UserRole:
                return node->name();
            case Qt::DecorationRole:
                return node->icon();
            case Qt::ToolTipRole:
                return node->tooltip();
            default:
                break;
        }
    }
    return QVariant();
}

Qt::ItemFlags PlotsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    if (auto node = static_cast<PlotsModelNode*>(index.internalPointer()); node != nullptr)
        return node->flags();
    return QAbstractItemModel::flags(index);
}

bool PlotsModel::removeRows(int row, int count, const QModelIndex& parent)
{
    bool result = true;
    if (auto node = static_cast<PlotsModelNode*>(parent.internalPointer()); node != nullptr)
    {
        beginRemoveRows(parent, row, row + count - 1);
        for (int index = row + count - 1; index >= row; --index)
        {
            auto child = node->child(index);
            if (child != nullptr && child->deletable())
                node->remove_child(index);
            else
                result = false;
        }
        endRemoveRows();
    }
    return result;
}

void PlotsModel::set_selected(const QList<QModelIndex>& indexes, bool selected)
{
    for (auto index : indexes)
    {
        auto node = static_cast<PlotsModelNode*>(index.internalPointer());
        if (node != nullptr)
            node->set_selected(selected);
    }
}

void PlotsModel::addTopLevelNode(QObject* obj)
{
    beginInsertRows(QModelIndex(), m_rootNode->children_count(), m_rootNode->children_count());
    m_rootNode->insert_child(obj);
    endInsertRows();
    Q_EMIT top_level_nodes_list_changed();
}

Q_APPLICATION_STATIC(PlotsModel, _plots_model);

PlotsModel* PlotsModel::instance()
{
    return _plots_model();
}

QObject* PlotsModel::object(const QModelIndex& index)
{
    if (auto node = static_cast<PlotsModelNode*>(index.internalPointer()); node != nullptr)
        return node->object();
    return nullptr;
}

QMimeData* PlotsModel::mimeData(const QModelIndexList& indexes) const
{
    auto mimeData = new QMimeData();
    return mimeData;
}

QModelIndex PlotsModel::make_index(PlotsModelNode* node)
{
    auto parent_node = node->parent_node();
    if (parent_node == nullptr)
        return QModelIndex();
    return createIndex(parent_node->child_row(node), 0, node);
}

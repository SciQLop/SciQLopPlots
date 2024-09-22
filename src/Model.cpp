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
#include <qapplicationstatic.h>

void PlotsModel::children_changed(PlotsModelNode* node)
{
    auto parent_node = node->parent_node();
    if (parent_node == nullptr)
        return;
    QModelIndex index = createIndex(parent_node->child_row(node), 0, node);
    emit dataChanged(index, index);
}

PlotsModel::PlotsModel(QObject* parent)
{
    m_rootNode = new PlotsModelNode(this);
    connect(m_rootNode, &PlotsModelNode::childrenChanged, this, &PlotsModel::children_changed);
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
    return QAbstractItemModel::flags(index);
}

void PlotsModel::select(const QList<QModelIndex>& indexes)
{
    for (auto index : indexes)
    {
        auto node = static_cast<PlotsModelNode*>(index.internalPointer());
        if (node != nullptr)
            node->set_selected(true);
    }
}

void PlotsModel::addTopLevelNode(QObject* obj)
{
    beginInsertRows(QModelIndex(), m_rootNode->children_count(), m_rootNode->children_count());
    m_rootNode->insert_child(obj);
    endInsertRows();
}

Q_APPLICATION_STATIC(PlotsModel, _plots_model);

PlotsModel* PlotsModel::instance()
{
    return _plots_model();
}

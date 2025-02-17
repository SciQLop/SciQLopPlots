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
#include "SciQLopPlots/Products/ProductsModel.hpp"
#include "SciQLopPlots/Products/ProductsNode.hpp"
#include <QIODevice>
#include <qapplicationstatic.h>

QModelIndex ProductsModel::make_index(ProductsModelNode* node)
{
    auto parent_node = node->parent_node();
    if (parent_node == nullptr)
        return QModelIndex();
    return createIndex(parent_node->child_row(node), 0, node);
}

void ProductsModel::_add_to_completer(const QString& value)
{
    if (m_completer_model->stringList().contains(value))
        return;
    m_completer_model->insertRow(m_completer_model->rowCount());
    m_completer_model->setData(m_completer_model->index(m_completer_model->rowCount() - 1), value);
}

void ProductsModel::_add_to_completer(ProductsModelNode* node)
{
    for (auto str : node->completions())
        _add_to_completer(str);
}

void ProductsModel::_insert_node(ProductsModelNode* node, ProductsModelNode* parent)
{
    beginInsertRows(make_index(parent), parent->children_count(), parent->children_count());
    parent->add_child(node);
    _add_to_completer(node);
    endInsertRows();
}

void ProductsModel::_add_text_mime_data(QMimeData* mime_data, const QModelIndexList& indexes) const
{
    QStringList paths;
    for (const auto& index : indexes)
    {
        if (index.isValid())
        {
            auto node = static_cast<ProductsModelNode*>(index.internalPointer());
            paths += node->path().join("//");
        }
    }
    mime_data->setText(paths.join("\n"));
}

ProductsModel::ProductsModel(QObject* parent) : QAbstractItemModel(parent)
{
    m_rootNode = new ProductsModelNode("root", {}, "", this);
    m_completer_model = new QStringListModel(this);
}

QModelIndex ProductsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (hasIndex(row, column, parent))
    {
        ProductsModelNode* parentNode;
        if (!parent.isValid())
            parentNode = m_rootNode;
        else
            parentNode = static_cast<ProductsModelNode*>(parent.internalPointer());
        ProductsModelNode* childNode = parentNode->child(row);
        if (childNode)
            return createIndex(row, column, childNode);
    }
    return QModelIndex();
}

QModelIndex ProductsModel::parent(const QModelIndex& index) const
{
    if (index.isValid())
    {
        ProductsModelNode* childNode = static_cast<ProductsModelNode*>(index.internalPointer());
        ProductsModelNode* parentNode = childNode->parent_node();
        if (parentNode != m_rootNode)
        {
            if (parentNode != nullptr)
            {
                ProductsModelNode* grandparentNode = parentNode->parent_node();
                if (grandparentNode != nullptr)
                    return createIndex(grandparentNode->child_row(parentNode), 0, parentNode);
                return createIndex(0, 0, parentNode);
            }
        }
    }
    return QModelIndex();
}

int ProductsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    ProductsModelNode* parentNode;
    if (!parent.isValid())
        parentNode = m_rootNode;
    else
        parentNode = static_cast<ProductsModelNode*>(parent.internalPointer());
    return parentNode->children_count();
}

int ProductsModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant ProductsModel::data(const QModelIndex& index, int role) const
{
    if (index.isValid())
    {
        ProductsModelNode* node = static_cast<ProductsModelNode*>(index.internalPointer());
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
            case PRODUCT_FILTER_ROLE:
                return node->raw_text();
            default:
                break;
        }
    }
    return QVariant();
}

Qt::ItemFlags ProductsModel::flags(const QModelIndex& index) const
{
    if (index.isValid())
    {
        auto node = static_cast<ProductsModelNode*>(index.internalPointer());
        if (node->node_type() == ProductsModelNodeType::PARAMETER)
            return Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    return QAbstractItemModel::flags(index);
}

QMimeData* ProductsModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.isEmpty())
        return nullptr;
    auto mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    for (const auto& index : indexes)
    {
        if (index.isValid())
        {
            auto node = static_cast<ProductsModelNode*>(index.internalPointer());
            stream << node->path();
        }
    }
    mimeData->setData(mime_type(), encodedData);
    _add_text_mime_data(mimeData, indexes);
    return mimeData;
}

QList<QStringList> ProductsModel::decode_mime_data(const QMimeData* mime_data)
{
    QList<QStringList> data;
    if (mime_data->hasFormat(mime_type()))
    {
        QByteArray encodedData = mime_data->data(mime_type());
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        while (!stream.atEnd())
        {
            QStringList path;
            stream >> path;
            data.append(path);
        }
    }
    return data;
}

void ProductsModel::add_node(QStringList path, ProductsModelNode* obj)
{
    auto parent = m_rootNode;
    for (const auto& name : path)
    {
        if (!name.isEmpty())
        {
            auto node = parent->child(name);
            if (node == nullptr)
            {
                node = new ProductsModelNode(name);
                _insert_node(node, parent);
            }
            parent = node;
        }
    }
    _insert_node(obj, parent);
}

ProductsModelNode* ProductsModel::node(const QStringList& path)
{
    if (!path.isEmpty())
    {
        auto parent = ProductsModel::instance()->m_rootNode;
        auto without_root = path;
        if (without_root.first().isEmpty() or without_root.first() == parent->name())
            without_root.removeFirst();
        for (const auto& name : without_root)
        {
            auto node = parent->child(name);
            if (node == nullptr)
                return nullptr;
            parent = node;
        }
        return parent;
    }
    return nullptr;
}

Q_APPLICATION_STATIC(ProductsModel, _products_model);

ProductsModel* ProductsModel::instance()
{
    return _products_model();
}

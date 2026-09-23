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
#include <QPointer>
#include <QDebug>
#include <QThread>
#include <qapplicationstatic.h>

QModelIndex ProductsModel::make_index(ProductsModelNode* node)
{
    auto parent_node = node->parent_node();
    if (parent_node == nullptr)
        return QModelIndex();
    return createIndex(parent_node->child_row(node), 0, node);
}

void ProductsModel::node_data_changed(ProductsModelNode* node, const QList<int>& roles)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this,
            [this, guarded = QPointer<ProductsModelNode>(node), roles]
            {
                if (guarded)
                    node_data_changed(guarded, roles);
            },
            Qt::QueuedConnection);
        return;
    }
    const auto index = make_index(node);
    Q_EMIT dataChanged(index, index, roles);
}

void ProductsModel::_remove_child(ProductsModelNode* parent, int row)
{
    beginRemoveRows(make_index(parent), row, row);
    auto* removed = parent->take_child(row);
    endRemoveRows();
    delete removed;
}

void ProductsModel::_insert_node(ProductsModelNode* node, ProductsModelNode* parent)
{
    // Re-publishing a same-named product replaces the old node. The removal
    // must be announced (begin/endRemoveRows) — a silent swap leaves views and
    // proxies believing one more row exists than the node holds.
    if (auto* existing = parent->child(node->name()); existing)
    {
        if (existing == node)
            return; // already in place — deleting it here would insert a dangling pointer
        _remove_child(parent, parent->child_row(existing));
    }
    beginInsertRows(make_index(parent), parent->children_count(), parent->children_count());
    parent->add_child(node);
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
                return node->display_name();
            case Qt::UserRole:
                // Identity, not presentation: UserRole is consumed as the node's
                // key, so it must stay the objectName that path lookup matches.
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

bool ProductsModel::add_node(QStringList path, ProductsModelNode* obj)
{
    // Filter models purge dangling nodes from synchronous begin/endRemoveRows
    // signals; emitted from another thread they would arrive queued, after
    // _insert_node already deleted the replaced node. Queue instead of blocking:
    // a blocking hop can deadlock when the GUI thread waits on the caller.
    if (QThread::currentThread() != thread())
    {
        if (!obj->moveToThread(thread()))
        {
            qWarning() << "ProductsModel::add_node: refusing" << obj->name()
                       << "- it cannot be moved to the model thread (already parented?)";
            return false;
        }
        QMetaObject::invokeMethod(this, [this, path, obj] { add_node(path, obj); },
                                  Qt::QueuedConnection);
        return true;
    }
    // Qt refuses to parent across threads, which would leave the node listed but unowned.
    if (obj->thread() != thread())
    {
        qWarning() << "ProductsModel::add_node: refusing" << obj->name()
                   << "- it lives in another thread and can only be moved by that thread";
        return false;
    }
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
    return true;
}

ProductsModelNode* ProductsModel::_resolve(const QStringList& path) const
{
    auto parent = m_rootNode;
    auto without_root = path;
    if (!without_root.isEmpty()
        && (without_root.first().isEmpty() or without_root.first() == parent->name()))
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

void ProductsModel::remove_node(QStringList path)
{
    // Same rule as add_node: mutate on the model thread only. Only the path crosses,
    // so unlike add_node there is nothing to move.
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, path] { remove_node(path); },
                                  Qt::QueuedConnection);
        return;
    }
    auto* node = _resolve(path);
    if (node == nullptr || node == m_rootNode)
        return;
    auto* parent = node->parent_node();
    _remove_child(parent, parent->child_row(node));
}

ProductsModelNode* ProductsModel::node(const QStringList& path)
{
    if (path.isEmpty())
        return nullptr;
    return ProductsModel::instance()->_resolve(path);
}

Q_APPLICATION_STATIC(ProductsModel, _products_model);

ProductsModel* ProductsModel::instance()
{
    return _products_model();
}

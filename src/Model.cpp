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
#include "SciQLopPlots/MultiPlots/SciQLopPlotPanelInterface.hpp"
#include <QByteArray>
#include <QMimeData>
#include <QPointer>
#include <qapplicationstatic.h>

namespace
{
constexpr auto k_plot_reorder_mime = "application/x-sciqlop-plot-reorder";

QObject* decode_reorder_payload(const QMimeData* data)
{
    if (!data || !data->hasFormat(k_plot_reorder_mime))
        return nullptr;
    bool ok = false;
    auto value = data->data(k_plot_reorder_mime).toULongLong(&ok);
    if (!ok)
        return nullptr;
    return reinterpret_cast<QObject*>(static_cast<quintptr>(value));
}
}

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

    QList<QPointer<QObject>> deferred_deletes;

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = row + count - 1; i >= row; --i)
    {
        auto child = parentNode->child(i);
        if (!child)
            continue;
        child->disconnect_all();
        QObject* obj = child->object();
        if (obj && child->deletable())
            deferred_deletes.append(obj);
        parentNode->remove_child(i);
        emit node_removed(obj);
        delete child;
    }
    endRemoveRows();

    for (auto& obj : deferred_deletes)
    {
        if (!obj)
            continue;
        if (auto panel = qobject_cast<SciQLopPlotPanelInterface*>(obj.data()))
            panel->request_delete();
        else
            obj->deleteLater();
    }

    return true;
}

void PlotsModel::addNode(PlotsModelNode* parent, QObject* obj)
{
    if (!obj)
        return;
    if (parent->child_row_by_object(obj) >= 0)
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
                if (r < 0)
                    return;
                // Object is already being destroyed — detach the node
                // without scheduling deleteLater on the dying object
                beginRemoveRows(make_index(guardedParent), r, r);
                guardedNode->disconnect_all();
                QObject* obj = guardedNode->object();
                guardedParent->remove_child(r);
                emit node_removed(obj);
                delete guardedNode.data();
                endRemoveRows();
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

    if (desc && desc->connect_moves)
    {
        auto move_child = [this, guardedNode](QObject* child, int dest_row)
        {
            if (guardedNode)
                moveChildByObject(guardedNode, child, dest_row);
        };
        node->add_connections(desc->connect_moves(obj, move_child));
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

void PlotsModel::moveChildByObject(PlotsModelNode* parent, QObject* obj, int dest_row)
{
    if (!parent || !obj)
        return;
    int from = parent->child_row_by_object(obj);
    if (from < 0)
        return;
    int count = parent->children_count();
    int to = std::clamp(dest_row, 0, count - 1);
    if (from == to)
        return;
    auto parent_idx = make_index(parent);
    // beginMoveRows uses Qt's "before this row" convention: moving down needs +1
    int qt_dest = (to > from) ? to + 1 : to;
    if (!beginMoveRows(parent_idx, from, from, parent_idx, qt_dest))
        return;
    auto* node = parent->child(from);
    parent->remove_child(from);
    parent->reinsert_child_at(node, to);
    endMoveRows();
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

QStringList PlotsModel::mimeTypes() const
{
    return { QString::fromLatin1(k_plot_reorder_mime) };
}

QMimeData* PlotsModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mime = new QMimeData();
    for (const auto& idx : indexes)
    {
        if (!idx.isValid())
            continue;
        auto* node = static_cast<PlotsModelNode*>(idx.internalPointer());
        if (!node || !node->object())
            continue;
        if (!(node->flags() & Qt::ItemIsDragEnabled))
            continue;
        auto value = static_cast<qulonglong>(reinterpret_cast<quintptr>(node->object()));
        mime->setData(k_plot_reorder_mime, QByteArray::number(value));
        break; // single-selection mode; only one payload
    }
    return mime;
}

Qt::DropActions PlotsModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

bool PlotsModel::canDropMimeData(const QMimeData* data, Qt::DropAction action,
    int /*row*/, int /*column*/, const QModelIndex& parent) const
{
    if (action != Qt::MoveAction)
        return false;
    auto* source = decode_reorder_payload(data);
    if (!source)
        return false;
    if (!parent.isValid())
        return false;
    auto* parent_node = static_cast<PlotsModelNode*>(parent.internalPointer());
    if (!parent_node)
        return false;
    auto* panel = qobject_cast<SciQLopPlotPanelInterface*>(parent_node->object());
    if (!panel)
        return false;
    return parent_node->child_row_by_object(source) >= 0;
}

bool PlotsModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
    int row, int /*column*/, const QModelIndex& parent)
{
    if (!canDropMimeData(data, action, row, 0, parent))
        return false;
    auto* plot = qobject_cast<SciQLopPlotInterface*>(decode_reorder_payload(data));
    auto* parent_node = static_cast<PlotsModelNode*>(parent.internalPointer());
    auto* panel = qobject_cast<SciQLopPlotPanelInterface*>(parent_node->object());
    if (!plot || !panel)
        return false;

    int plot_count = static_cast<int>(panel->plots().size());
    int from = panel->index(plot);
    if (from < 0)
        return false;

    // Qt passes an insertion-slot row (before-this-row, or -1 for "on parent").
    // Collapse to a final plot-list index, clamped to the plot range so drops
    // over extension rows still resolve to a valid plot position.
    int to;
    if (row < 0 || row >= plot_count)
        to = plot_count - 1;
    else if (row > from)
        to = row - 1;
    else
        to = row;

    if (from == to)
        return false;

    panel->move_plot(plot, to);
    // Model row update is driven by the panel's plot_moved signal,
    // wired in Registrations via TypeDescriptor::connect_moves.
    return true;
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

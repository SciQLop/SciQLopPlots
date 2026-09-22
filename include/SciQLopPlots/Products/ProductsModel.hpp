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
#include "SciQLopPlots/Products/ProductsNode.hpp"
#include <QAbstractItemModel>
#include <QMimeData>
#include <QObject>

inline constexpr auto PRODUCT_FILTER_ROLE = Qt::UserRole + 1;

class ProductsModel : public QAbstractItemModel
{
    Q_OBJECT
    ProductsModelNode* m_rootNode;

    QModelIndex make_index(ProductsModelNode* node);

    void _insert_node(ProductsModelNode* node, ProductsModelNode* parent);

    //! Announces the removal before the node is freed: the filter models purge their
    //! per-node state from rowsAboutToBeRemoved, so `delete` must come last.
    void _remove_child(ProductsModelNode* parent, int row);

    //! Walks \a path like node() does: empty segments and one leading root name are skipped.
    ProductsModelNode* _resolve(const QStringList& path) const;

    void _add_text_mime_data(QMimeData* mime_data, const QModelIndexList& indexes) const;

public:
    ProductsModel(QObject* parent = nullptr);
    ~ProductsModel() = default;

    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QMimeData* mimeData(const QModelIndexList& indexes) const override;

    static QList<QStringList> decode_mime_data(const QMimeData* mime_data);

    /*!
     * \brief add_node Insert \a obj under \a path, replacing a same-named node.
     *
     * Called from another thread it does not block: \a obj is moved to the model
     * thread and inserted later, so node() and rowCount() only see it once the
     * model thread has processed events. A node that cannot be moved (it already
     * has a QObject parent) is refused with a warning.
     *
     * \return true if \a obj was inserted, or queued to be inserted on the model
     * thread; false if it was refused. The caller keeps ownership on false.
     */
    Q_SLOT bool add_node(QStringList path, ProductsModelNode* obj);

    /*!
     * \brief remove_node Delete the node at \a path and everything below it.
     *
     * The path is read like node()'s, so `remove_node(node.path())` works. A missing
     * path, or the root, is ignored. Empty parent folders are left in place: a folder
     * may have been published on purpose, remove it explicitly if unwanted.
     * Called from another thread it does not block: the removal runs later on the
     * model thread, so node() only stops finding it once that thread has processed
     * events. Python wrappers of the removed nodes become invalid.
     */
    Q_SLOT void remove_node(QStringList path);

    static ProductsModelNode* node(const QStringList& path);

    inline static QString mime_type() { return "application/x-product"; }

    static ProductsModel* instance();
};

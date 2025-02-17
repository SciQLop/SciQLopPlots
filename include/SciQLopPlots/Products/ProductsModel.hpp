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
#include <QStringListModel>

inline constexpr auto PRODUCT_FILTER_ROLE = Qt::UserRole + 1;

class ProductsModel : public QAbstractItemModel
{
    Q_OBJECT
    ProductsModelNode* m_rootNode;
    QStringListModel* m_completer_model;

    QModelIndex make_index(ProductsModelNode* node);

    void _add_to_completer(const QString& value);
    void _add_to_completer(ProductsModelNode* node);

    void _insert_node(ProductsModelNode* node, ProductsModelNode* parent);

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

    inline QStringListModel* completer_model() const { return m_completer_model; }

    Q_SLOT void add_node(QStringList path, ProductsModelNode* obj);

    static ProductsModelNode* node(const QStringList& path);

    inline static QString mime_type() { return "application/x-product"; }

    static ProductsModel* instance();
};

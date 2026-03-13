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
#include "SciQLopPlots/Products/QueryParser.hpp"
#include <QAbstractListModel>
#include <QMimeData>
#include <QTimer>

class ProductsModel;
class ProductsModelNode;

class ProductsFlatFilterModel : public QAbstractListModel
{
    Q_OBJECT
    ProductsModel* m_source;
    Query m_query;

    struct ScoredNode
    {
        ProductsModelNode* node;
        int score;
    };
    QList<ScoredNode> m_results;

    QList<ProductsModelNode*> m_pending_leaves;
    int m_batch_cursor = 0;
    int m_batch_generation = 0;
    QTimer* m_batch_timer;

    static constexpr int BATCH_SIZE = 200;

public:
    ProductsFlatFilterModel(ProductsModel* source, QObject* parent = nullptr);

    void set_query(const Query& query);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    QStringList mimeTypes() const override;
    Qt::DropActions supportedDragActions() const override;

private:
    void rebuild();
    void collect_all_leaves(ProductsModelNode* node, QList<ProductsModelNode*>& out) const;
    void process_batch();
    bool filters_match(ProductsModelNode* node) const;
    int free_text_score(ProductsModelNode* node) const;
};

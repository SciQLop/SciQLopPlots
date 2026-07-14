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
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include <QHash>
#include <QSortFilterProxyModel>

class ProductsModelNode;

class ProductsTreeFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Query m_query;
    int m_max_score_tiers = 2;
    int m_score_cutoff = 0;
    int m_max_score = 0;

    struct CoverageInfo
    {
        int matched = 0;
        int total = 0;
    };
    QHash<ProductsModelNode*, CoverageInfo> m_coverage;

public:
    ProductsTreeFilterModel(QObject* parent = nullptr);

    void set_query(const Query& query);

    // The tree has no ranked/flat view to fall back on, so unlike
    // ProductsFlatFilterModel it needs an explicit notion of "how many
    // distinct score tiers count as a match" — a raw score threshold would
    // be meaningless across queries/corpora (see ProductsFlatFilterModel's
    // free_text_score: the scale is small, coarse, and length-dependent).
    void set_max_score_tiers(int max_tiers);
    int max_score_tiers() const noexcept { return m_max_score_tiers; }

    QVariant data(const QModelIndex& index, int role) const override;

    void setSourceModel(QAbstractItemModel* source_model) override;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    bool node_matches(ProductsModelNode* node) const;
    bool filters_match(ProductsModelNode* node) const;
    int free_text_score(ProductsModelNode* node) const;
    void collect_all_leaves(ProductsModelNode* node, QList<ProductsModelNode*>& out) const;
    void recompute_score_cutoff();
    void recompute_total_leaf_counts();
    void on_source_structure_changed();
};

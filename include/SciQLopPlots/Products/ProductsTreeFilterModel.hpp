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
#include "SciQLopPlots/Products/ExternalScoreOverlay.hpp"
#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVariant>

class ProductsModelNode;

class ProductsTreeFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

    static constexpr int BATCH_SIZE = 200;

    // Committed state: what filterAcceptsRow()/data() currently expose. Only
    // ever updated atomically, all-at-once, by finish_scoring() -- so a
    // reader never sees scores for one query mixed with a cutoff for
    // another.
    Query m_query;
    int m_max_score_tiers = 2;
    int m_score_cutoff = 0;
    int m_max_score = 0;
    QHash<ProductsModelNode*, int> m_node_scores;
    QSet<int> m_distinct_scores;

    struct CoverageInfo
    {
        int matched = 0;
        int total = 0;
    };
    QHash<ProductsModelNode*, CoverageInfo> m_coverage;
    ExternalScoreOverlay m_external_scores;

    // Pending state: in-flight background scoring for a query that hasn't
    // been committed yet. filterAcceptsRow()/data() never read these --
    // the view keeps showing the previous committed results until
    // finish_scoring() swaps pending into committed in one shot, avoiding
    // the old design's synchronous full-corpus walk (up to 3x per
    // keystroke) on the UI thread.
    Query m_pending_query;
    QList<ProductsModelNode*> m_pending_leaves;
    QHash<ProductsModelNode*, int> m_pending_scores;
    QSet<int> m_pending_distinct_scores;
    int m_pending_max_score = 0;
    int m_batch_cursor = 0;
    int m_batch_generation = 0;
    QTimer* m_batch_timer;

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

    void set_external_scores(const QHash<QString, QVariant>& scores)
    {
        m_external_scores.set_scores(scores);
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_batch_timer->isActive())
                    set_query(m_query);
            },
            Qt::QueuedConnection);
    }
    void set_smart_search_enabled(bool enabled) { m_external_scores.set_enabled(enabled); }
    bool smart_search_enabled() const noexcept { return m_external_scores.enabled(); }

    QVariant data(const QModelIndex& index, int role) const override;

    void setSourceModel(QAbstractItemModel* source_model) override;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    bool filters_match(ProductsModelNode* node, const Query& query) const;
    int free_text_score(ProductsModelNode* node, const Query& query) const;
    void collect_all_leaves(ProductsModelNode* node, QList<ProductsModelNode*>& out) const;
    void recompute_total_leaf_counts();
    void on_source_structure_changed();

    // Kicks off (or restarts) chunked background scoring of every leaf
    // against m_pending_query, BATCH_SIZE nodes per 0ms QTimer tick so the
    // UI thread never blocks for the whole corpus at once.
    void start_scoring();
    void process_score_batch();
    // Atomically swaps the finished pending scoring results into the
    // committed state and reveals them with a single filter invalidation.
    void finish_scoring();
    void apply_cutoff_and_coverage(const QHash<ProductsModelNode*, int>& scores,
                                    const QSet<int>& distinct_scores, int max_score);
};

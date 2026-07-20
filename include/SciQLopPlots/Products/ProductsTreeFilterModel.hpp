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
#include "SciQLopPlots/Products/ScoreSignalRegistry.hpp"
#include "SciQLopPlots/Products/ScoreMerge.hpp"
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
    double m_score_cutoff = 0;
    double m_max_score = 0;
    QHash<ProductsModelNode*, double> m_node_scores;
    QSet<double> m_distinct_scores;

    struct CoverageInfo
    {
        int matched = 0;
        int total = 0;
    };
    QHash<ProductsModelNode*, CoverageInfo> m_coverage;

    ScoreSignalRegistry m_score_signals;
    ScoreMergeStrategy m_merge_strategy = ScoreMergeStrategy::Max;
    QHash<QString, double> m_signal_weights;
    QString m_override_signal;

    // Raw per-signal scores from the last completed scoring pass, kept
    // after settling so set_score_merge_strategy()/set_signal_weight()/
    // set_override_signal() -- like set_max_score_tiers() already does --
    // can cheaply re-rank without re-running the DP scorer.
    QHash<ProductsModelNode*, QHash<QString, double>> m_node_raw_signals;
    QHash<QString, double> m_signal_maxes;

    // Pending state: in-flight background scoring for a query that hasn't
    // been committed yet. filterAcceptsRow()/data() never read these --
    // the view keeps showing the previous committed results until
    // finish_scoring() swaps pending into committed in one shot.
    Query m_pending_query;
    QList<ProductsModelNode*> m_pending_leaves;
    QHash<ProductsModelNode*, QHash<QString, double>> m_pending_raw_signals;
    QHash<QString, double> m_pending_signal_maxes;
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

    void set_external_scores(const QString& signal_name, const QHash<QString, QVariant>& scores)
    {
        m_score_signals.set_scores(signal_name, scores);
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_batch_timer->isActive())
                    set_query(m_query);
            },
            Qt::QueuedConnection);
    }
    void set_signal_enabled(const QString& signal_name, bool enabled)
    {
        m_score_signals.set_signal_enabled(signal_name, enabled);
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_batch_timer->isActive())
                    set_query(m_query);
            },
            Qt::QueuedConnection);
    }
    bool signal_enabled(const QString& signal_name) const
    {
        return m_score_signals.signal_enabled(signal_name);
    }
    QStringList registered_signals() const { return m_score_signals.registered_signals(); }

    void set_score_merge_strategy(ScoreMergeStrategy strategy)
    {
        m_merge_strategy = strategy;
        remerge_committed();
    }
    ScoreMergeStrategy score_merge_strategy() const noexcept { return m_merge_strategy; }

    void set_signal_weight(const QString& signal_name, double weight)
    {
        m_signal_weights.insert(signal_name, weight);
        remerge_committed();
    }
    double signal_weight(const QString& signal_name) const
    {
        return m_signal_weights.value(signal_name, 1.0);
    }

    void set_override_signal(const QString& signal_name)
    {
        m_override_signal = signal_name;
        remerge_committed();
    }
    QString override_signal() const { return m_override_signal; }

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
    void apply_cutoff_and_coverage(const QHash<ProductsModelNode*, double>& scores,
                                    const QSet<double>& distinct_scores, double max_score);
    void remerge_committed();
};

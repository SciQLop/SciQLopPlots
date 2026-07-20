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
#include <QAbstractListModel>
#include <QMimeData>
#include <QTimer>
#include <QVariant>

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
        double score;
    };
    QList<ScoredNode> m_results;
    double m_max_score = 0;

    ScoreSignalRegistry m_score_signals;
    ScoreMergeStrategy m_merge_strategy = ScoreMergeStrategy::Max;
    QHash<QString, double> m_signal_weights;
    QString m_override_signal;

    // Per-leaf raw signal scores from the last completed text-scoring
    // pass, kept after settling so set_score_merge_strategy()/
    // set_signal_weight()/set_override_signal() can cheaply re-merge
    // without re-running the DP scorer over the whole corpus -- mirrors
    // ProductsTreeFilterModel's set_max_score_tiers cheap-rerank pattern.
    QHash<ProductsModelNode*, QHash<QString, double>> m_node_raw_signals;
    QHash<QString, double> m_signal_maxes;

    struct LeafEntry
    {
        ProductsModelNode* node;
        QString path_text;
        QString meta_text;
    };
    QList<LeafEntry> m_pending_leaves;
    int m_batch_cursor = 0;
    int m_batch_generation = 0;
    QTimer* m_batch_timer;

    // Scratch state accumulated across batches; committed into
    // m_node_raw_signals/m_signal_maxes only once the whole corpus has
    // been scanned (see finalize_batch()).
    QHash<ProductsModelNode*, QHash<QString, double>> m_pending_raw_signals;
    QHash<QString, double> m_pending_signal_maxes;

    static constexpr int BATCH_SIZE = 200;

public:
    ProductsFlatFilterModel(ProductsModel* source, QObject* parent = nullptr);

    void set_query(const Query& query);

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

    // Full unfiltered corpus, independent of the current query -- used by
    // SmartSearchController (Python) to (re)index a search method without
    // needing to walk ProductsModelNode itself.
    QHash<QString, QString> corpus_snapshot() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    QStringList mimeTypes() const override;
    Qt::DropActions supportedDragActions() const override;

private:
    void rebuild();
    void collect_all_leaves(ProductsModelNode* node, QList<LeafEntry>& out) const;
    void process_batch();
    void finalize_batch();
    void remerge_committed();
    void sort_and_remap_results();
    bool filters_match(ProductsModelNode* node) const;
    // Scores path and metadata text separately and keeps the max per token
    // instead of concatenating them into one candidate string -- otherwise a
    // node with rich metadata (e.g. CDAWeb's verbose ISTP-style attributes)
    // gets its length penalty inflated by text that has nothing to do with
    // whether the path itself is a clean match.
    int free_text_score(const QString& path_text, const QString& meta_text) const;
};

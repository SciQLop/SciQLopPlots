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
#include "SciQLopPlots/Products/ScoreMerge.hpp"
#include <QObject>
#include <QStringList>
#include <QWidget>

class QueryLineEdit;
class QTreeView;
class QListView;
class QStackedWidget;
class QToolButton;
class QTextBrowser;
class QLabel;
class QTimer;
class ProductsTreeFilterModel;
class ProductsFlatFilterModel;
class ProductsScoreDelegate;
struct Query;

class ProductsView : public QWidget
{
    Q_OBJECT
    QueryLineEdit* m_query_line_edit;
    QTreeView* m_tree_view;
    QListView* m_list_view;
    QStackedWidget* m_stack;
    QToolButton* m_view_toggle;
    QLabel* m_result_count;
    ProductsTreeFilterModel* m_tree_filter;
    ProductsFlatFilterModel* m_flat_filter;
    ProductsScoreDelegate* m_score_delegate;
    QTimer* m_completion_refresh_timer;

    Q_SLOT void on_query_changed(const Query& query);
    void toggle_view();
    void update_result_count();
    void refresh_completions();

public:
    ProductsView(QWidget* parent = nullptr);
    virtual ~ProductsView() = default;

    void set_search_help(const QString& html);
    QString search_help() const;

    // Forwards to both internal filter models, keeping them in sync so
    // toggling between tree/list view mid-session never loses signal state.
    // Getters read m_tree_filter only -- both are always kept configured
    // identically by construction, so either is a valid source of truth.
    void set_external_scores(const QString& signal_name, const QHash<QString, QVariant>& scores);
    void set_signal_enabled(const QString& signal_name, bool enabled);
    bool signal_enabled(const QString& signal_name) const;
    QStringList registered_signals() const;

    void set_score_merge_strategy(ScoreMergeStrategy strategy);
    ScoreMergeStrategy score_merge_strategy() const;

    void set_signal_weight(const QString& signal_name, double weight);
    double signal_weight(const QString& signal_name) const;

    void set_override_signal(const QString& signal_name);
    QString override_signal() const;

    // Emitted whenever the query bar's free-text tokens change (filter-only
    // syntax like "provider:x" is excluded, same split QueryParser already
    // does) -- lets a Python search controller (re)dispatch an async
    // external-scoring query (e.g. BM25) without polling or reparsing the
    // query bar itself. Emitted with an empty list when the query is
    // cleared, so a controller knows to drop stale external scores too.
    // Declared last in the class: BINDINGS_H's shiboken-only `signals:`
    // switch (see SciQLopOverlay.hpp/SciQLopPlotInterface.hpp for the same
    // pattern) changes the access specifier for everything after it, so
    // nothing below this may need to stay a plain callable method.
#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void free_text_query_changed(const QStringList& tokens);
};

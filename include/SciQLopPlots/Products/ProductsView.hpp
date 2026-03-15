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
#include <QObject>
#include <QWidget>

class QueryLineEdit;
class QTreeView;
class QListView;
class QStackedWidget;
class QToolButton;
class QLabel;
class QTimer;
class ProductsTreeFilterModel;
class ProductsFlatFilterModel;
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
    QTimer* m_completion_refresh_timer;

    Q_SLOT void on_query_changed(const Query& query);
    void toggle_view();
    void update_result_count();
    void refresh_completions();

public:
    ProductsView(QWidget* parent = nullptr);
    virtual ~ProductsView() = default;
};

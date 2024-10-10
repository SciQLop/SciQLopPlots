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
#include "SciQLopPlots/Products/ProductsView.hpp"
#include "SciQLopPlots/Products/ProductsModel.hpp"
#include <QCompleter>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <QWidget>

void ProductsView::update_filter()
{
    m_proxy_model->setFilterRegularExpression(
        QRegularExpression(".*" + QRegularExpression::escape(m_search_line_edit->text()) + ".*"));
}

ProductsView::ProductsView(QWidget* parent)
{
    this->setWindowTitle("Products");
    auto layout = new QVBoxLayout(this);
    m_search_timer = new QTimer(this);
    m_search_timer->setSingleShot(true);
    m_search_timer->setInterval(500);
    connect(m_search_timer, &QTimer::timeout, this, &ProductsView::update_filter);

    m_search_line_edit = new QLineEdit(this);
    m_search_line_edit->setClearButtonEnabled(true);
    m_search_line_edit->setPlaceholderText("Search...");
    m_search_line_edit->addAction(QIcon(":/icons/theme/search.png"), QLineEdit::LeadingPosition);
    m_completer = new QCompleter(this);
    m_completer->setModel(ProductsModel::instance()->completer_model());
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    m_search_line_edit->setCompleter(m_completer);
    layout->addWidget(m_search_line_edit);

    m_proxy_model = new QSortFilterProxyModel(this);
    m_proxy_model->setSourceModel(ProductsModel::instance());
    m_proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy_model->setFilterKeyColumn(0);
    m_proxy_model->setFilterRole(PRODUCT_FILTER_ROLE);
    m_proxy_model->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy_model->setRecursiveFilteringEnabled(true);
    m_proxy_model->setDynamicSortFilter(true);

    connect(m_search_line_edit, &QLineEdit::textChanged, this,
            [this]() { m_search_timer->start(); });


    m_tree_view = new QTreeView(this);
    m_tree_view->setModel(m_proxy_model);
    m_tree_view->setHeaderHidden(true);
    m_tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree_view->setRootIsDecorated(true);
    m_tree_view->setAlternatingRowColors(true);
    m_tree_view->setDragEnabled(true);
    layout->addWidget(m_tree_view);
    this->setLayout(layout);
}

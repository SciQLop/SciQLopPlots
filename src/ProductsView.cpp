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
#include "SciQLopPlots/Products/ProductsFlatFilterModel.hpp"
#include "SciQLopPlots/Products/ProductsModel.hpp"
#include "SciQLopPlots/Products/ProductsTreeFilterModel.hpp"
#include "SciQLopPlots/Products/QueryLineEdit.hpp"
#include "SciQLopPlots/Products/QueryParser.hpp"
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <magic_enum/magic_enum.hpp>

ProductsView::ProductsView(QWidget* parent) : QWidget(parent)
{
    this->setWindowTitle("Products");
    auto* layout = new QVBoxLayout(this);

    m_query_line_edit = new QueryLineEdit(this);
    layout->addWidget(m_query_line_edit);

    auto* toolbar = new QHBoxLayout();
    m_view_toggle = new QToolButton(this);
    m_view_toggle->setText("List");
    m_view_toggle->setToolTip("Toggle tree/list view");
    m_view_toggle->setCheckable(true);
    m_view_toggle->hide();
    toolbar->addWidget(m_view_toggle);

    m_result_count = new QLabel(this);
    m_result_count->hide();
    toolbar->addWidget(m_result_count);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_stack = new QStackedWidget(this);

    m_tree_filter = new ProductsTreeFilterModel(this);
    m_tree_filter->setSourceModel(ProductsModel::instance());

    m_tree_view = new QTreeView(this);
    m_tree_view->setModel(m_tree_filter);
    m_tree_view->setHeaderHidden(true);
    m_tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree_view->setRootIsDecorated(true);
    m_tree_view->setAlternatingRowColors(true);
    m_tree_view->setDragEnabled(true);
    m_stack->addWidget(m_tree_view);

    m_flat_filter = new ProductsFlatFilterModel(ProductsModel::instance(), this);

    m_list_view = new QListView(this);
    m_list_view->setModel(m_flat_filter);
    m_list_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list_view->setAlternatingRowColors(true);
    m_list_view->setDragEnabled(true);
    m_stack->addWidget(m_list_view);

    m_stack->setCurrentWidget(m_tree_view);
    layout->addWidget(m_stack);

    connect(m_query_line_edit, &QueryLineEdit::queryChanged, this,
            &ProductsView::on_query_changed);
    connect(m_view_toggle, &QToolButton::toggled, this,
            [this](bool) { toggle_view(); });

    m_completion_refresh_timer = new QTimer(this);
    m_completion_refresh_timer->setSingleShot(true);
    m_completion_refresh_timer->setInterval(1000);
    connect(m_completion_refresh_timer, &QTimer::timeout, this,
            &ProductsView::refresh_completions);
    connect(ProductsModel::instance(), &QAbstractItemModel::rowsInserted, this,
            [this]() { m_completion_refresh_timer->start(); });

    refresh_completions();
}

void ProductsView::on_query_changed(const Query& query)
{
    bool has_query = !query.free_text_tokens.isEmpty() || !query.filters.isEmpty();

    m_tree_filter->set_query(query);
    m_flat_filter->set_query(query);

    if (has_query)
    {
        m_view_toggle->show();
        m_result_count->show();
        if (!m_view_toggle->isChecked())
            m_stack->setCurrentWidget(m_list_view);
    }
    else
    {
        m_view_toggle->hide();
        m_result_count->hide();
        m_stack->setCurrentWidget(m_tree_view);
    }

    update_result_count();
}

void ProductsView::toggle_view()
{
    if (m_stack->currentWidget() == m_tree_view)
        m_stack->setCurrentWidget(m_list_view);
    else
        m_stack->setCurrentWidget(m_tree_view);
}

void ProductsView::update_result_count()
{
    int count = m_flat_filter->rowCount();
    m_result_count->setText(QString("%1 result%2").arg(count).arg(count != 1 ? "s" : ""));
}

void ProductsView::refresh_completions()
{
    QStringList field_names = { "provider", "type", "after", "before" };
    QMap<QString, QStringList> field_values;
    QStringList product_names;

    QStringList type_values;
    for (auto val : magic_enum::enum_values<ParameterType>())
    {
        if (val != ParameterType::NotAParameter)
            type_values.append(
                QString::fromStdString(std::string(magic_enum::enum_name(val))).toLower());
    }
    field_values["type"] = type_values;

    QSet<QString> providers;
    QSet<QString> metadata_keys;

    std::function<void(ProductsModelNode*)> walk = [&](ProductsModelNode* node) {
        if (node->node_type() == ProductsModelNodeType::PARAMETER)
        {
            product_names.append(node->name());
            if (!node->provider().isEmpty())
                providers.insert(node->provider());
            for (auto it = node->metadata().constBegin(); it != node->metadata().constEnd();
                 ++it)
            {
                if (!field_names.contains(it.key()))
                    metadata_keys.insert(it.key());
                auto val_str = it.value().toString();
                if (!val_str.isEmpty())
                    field_values[it.key()].append(val_str);
            }
        }
        for (auto* child : node->children_nodes())
            walk(child);
    };

    auto* model = ProductsModel::instance();
    for (int i = 0; i < model->rowCount(); ++i)
    {
        auto idx = model->index(i, 0);
        auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
        if (node)
            walk(node);
    }

    field_values["provider"] = providers.values();
    for (const auto& key : metadata_keys)
        field_names.append(key);

    for (auto it = field_values.begin(); it != field_values.end(); ++it)
        it.value().removeDuplicates();

    m_query_line_edit->set_completions(field_names, field_values, product_names);

    QSet<QString> known_fields_set;
    for (const auto& f : field_names)
        known_fields_set.insert(f.toLower());
    m_query_line_edit->set_known_fields(known_fields_set);
}

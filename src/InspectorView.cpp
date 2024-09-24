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
#include "SciQLopPlots/Inspector/View/InspectorView.hpp"
#include "SciQLopPlots/Inspector/Model/Model.hpp"
#include <QSplitter>
#include <QVBoxLayout>

InspectorView::InspectorView(QWidget* parent) : QWidget(parent)
{
    m_treeView = new PlotsTreeView(this);
    m_treeView->setModel(PlotsModel::instance());
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_treeView);
    setLayout(layout);

    auto selectionModel = m_treeView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::selectionChanged, PlotsModel::instance(),
        [](const QItemSelection& selected, const QItemSelection& deselected)
        {
            PlotsModel::instance()->set_selected(selected.indexes(), true);
            PlotsModel::instance()->set_selected(deselected.indexes(), false);
        });

    connect(PlotsModel::instance(), &PlotsModel::item_selection_changed, selectionModel,
        [selectionModel](const QModelIndex& index, bool selected)
        {
            if (selected)
                selectionModel->select(index, QItemSelectionModel::Select);
            else
                selectionModel->select(index, QItemSelectionModel::Deselect);
        });
}

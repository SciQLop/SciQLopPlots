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
#include "SciQLopPlots/Inspector/View/TreeView.hpp"
#include <QHeaderView>
#include <QKeyEvent>

PlotsTreeView::PlotsTreeView(QWidget* parent) : QTreeView(parent)
{
    header()->setVisible(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
}

void PlotsTreeView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete)
    {
        QModelIndexList indexes = selectionModel()->selectedIndexes();
        for (const QModelIndex& index : indexes)
        {
            if (index.isValid())
            {
                model()->removeRow(index.row(), index.parent());
            }
        }
    }
    else
    {
        QTreeView::keyPressEvent(event);
    }
}

void PlotsTreeView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::RightButton)
    {
        auto index = indexAt(event->pos());
        if (!index.isValid())
            clearSelection();
    }
    QTreeView::mousePressEvent(event);
}

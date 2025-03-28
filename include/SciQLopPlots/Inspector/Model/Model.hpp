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
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include <QAbstractItemModel>
#include <QObject>


class PlotsModel : public QAbstractItemModel
{
    Q_OBJECT
    PlotsModelNode* m_rootNode;

    Q_SLOT void node_changed();
    Q_SLOT void node_selection_changed(bool selected);
    QModelIndex make_index(PlotsModelNode* node);

    void addNode(PlotsModelNode*parent, QObject* obj);
    Q_SLOT void updateNodeChildren();
public:
    PlotsModel(QObject* parent = nullptr);
    ~PlotsModel() = default;

    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRow(int row, const QModelIndex& parent = QModelIndex());

    void set_selected(const QList<QModelIndex>& indexes, bool selected);

    Q_SLOT void addTopLevelNode(QObject* obj);

    static PlotsModel* instance();
    static QObject* object(const QModelIndex& index);

    QMimeData* mimeData(const QModelIndexList& indexes) const override;


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void item_selection_changed(const QModelIndex& index, bool selected);
    Q_SIGNAL void top_level_nodes_list_changed();

};

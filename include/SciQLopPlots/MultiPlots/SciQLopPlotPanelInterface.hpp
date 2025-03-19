/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlotCollection.hpp"

#include <QGridLayout>
#include <QScrollArea>
#include <QUuid>
#include <QWidget>

class SciQLopPlotContainer;
class SciQLopPlot;
class PlaceHolderManager;

class SciQLopPlotPanelInterface : public QScrollArea, public SciQLopPlotCollectionInterface
{
    Q_OBJECT

public:
    SciQLopPlotPanelInterface(QWidget* parent = nullptr):QScrollArea{parent}{}
    virtual ~SciQLopPlotPanelInterface() {}

    virtual inline void add_panel(SciQLopPlotPanelInterface* panel) { WARN_ABSTRACT_METHOD }

    virtual inline void insert_panel(int index, SciQLopPlotPanelInterface* panel)
    {
        WARN_ABSTRACT_METHOD
    }

    virtual inline void remove_panel(SciQLopPlotPanelInterface* panel)
    {
        WARN_ABSTRACT_METHOD
    }

    virtual inline void move_panel(int from, int to) { WARN_ABSTRACT_METHOD }

    virtual inline void move_panel(SciQLopPlotPanelInterface* panel, int to)
    {
        WARN_ABSTRACT_METHOD
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
    signals:
#endif
    Q_SIGNAL void plot_list_changed(const QList<QPointer<SciQLopPlotInterface>>& plots);
    Q_SIGNAL void plot_added(SciQLopPlotInterface* plot);
    Q_SIGNAL void plot_removed(SciQLopPlotInterface* plot);
    Q_SIGNAL void plot_moved(SciQLopPlotInterface* plot, int to);
    Q_SIGNAL void plot_inserted(SciQLopPlotInterface* plot, int at);
    Q_SIGNAL void selectionChanged(bool selected);
    Q_SIGNAL void time_range_changed(SciQLopPlotRange range);
};

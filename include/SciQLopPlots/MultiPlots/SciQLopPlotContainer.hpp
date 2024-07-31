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

#include <QSplitter>

class SciQLopPlot;

class SciQLopPlotContainer : public QSplitter
{
    Q_OBJECT

    QList<SciQLopPlot*> _plots;

public:
    SciQLopPlotContainer(QWidget* parent = nullptr);
    virtual ~SciQLopPlotContainer() Q_DECL_OVERRIDE = default;

    void insertWidget(int index, QWidget* widget);
    void addWidget(QWidget* widget);
    void insertPlot(int index, SciQLopPlot* plot);
    void addPlot(SciQLopPlot* plot);
    void removePlot(SciQLopPlot* plot, bool destroy = true);

    inline const QList<SciQLopPlot*>& plots() const { return _plots; }

    inline bool empty() const { return _plots.isEmpty(); }
    inline std::size_t count() const { return _plots.size(); }

    void setXAxisRange(double lower, double upper);

#ifndef BINDINGS_H
    Q_SIGNAL void plotAdded(SciQLopPlot* plot);
    Q_SIGNAL void plotRemoved(SciQLopPlot* plot);
    Q_SIGNAL void plotListChanged();
#endif // BINDINGS_H
};

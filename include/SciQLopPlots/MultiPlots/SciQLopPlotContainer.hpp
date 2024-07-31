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

#include "SciQLopPlotCollection.hpp"

class SciQLopPlot;

class SciQLopPlotContainer : public QSplitter, public SciQLopPlotCollectionInterface
{
    Q_OBJECT
    SciQLopPlotCollection* _plots;

public:
    SciQLopPlotContainer(QWidget* parent = nullptr);
    virtual ~SciQLopPlotContainer() Q_DECL_OVERRIDE = default;

    void insertWidget(int index, QWidget* widget);
    void addWidget(QWidget* widget);
    void insertPlot(int index, SciQLopPlot* plot) Q_DECL_OVERRIDE;
    void addPlot(SciQLopPlot* plot) Q_DECL_OVERRIDE;
    void movePlot(int from, int to) Q_DECL_OVERRIDE;
    void movePlot(SciQLopPlot* plot, int to) Q_DECL_OVERRIDE;
    void removePlot(SciQLopPlot* plot) Q_DECL_OVERRIDE;
    void removePlot(SciQLopPlot* plot, bool destroy);
    void removeWidget(QWidget* widget, bool destroy);

    virtual void clear() Q_DECL_OVERRIDE;

    inline virtual SciQLopPlot* plotAt(int index) Q_DECL_OVERRIDE { return _plots->plotAt(index); }
    inline virtual const QList<SciQLopPlot*>& plots() const Q_DECL_OVERRIDE
    {
        return _plots->plots();
    }

    inline virtual bool contains(SciQLopPlot* plot) const Q_DECL_OVERRIDE
    {
        return _plots->contains(plot);
    }

    inline virtual bool empty() const Q_DECL_OVERRIDE { return _plots->empty(); }
    virtual std::size_t size() const Q_DECL_OVERRIDE { return _plots->size(); }


    void setXAxisRange(double lower, double upper);

#ifndef BINDINGS_H
    Q_SIGNAL void plotListChanged(const QList<SciQLopPlot*>& plots);
#endif // BINDINGS_H
};

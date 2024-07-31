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

#include <QList>
#include <QObject>

class SciQLopPlot;

class SciQLopPlotCollectionInterface
{
public:
    virtual void addPlot(SciQLopPlot* plot) = 0;
    virtual void insertPlot(int index, SciQLopPlot* plot) = 0;
    virtual void removePlot(SciQLopPlot* plot) = 0;
    virtual void movePlot(int from, int to) = 0;
    virtual void movePlot(SciQLopPlot* plot, int to) = 0;
    virtual void clear() = 0;

    virtual SciQLopPlot* plotAt(int index) = 0;
    virtual const QList<SciQLopPlot*>& plots() const = 0;

    virtual bool contains(SciQLopPlot* plot) const = 0;

    virtual bool empty() const = 0;
    virtual std::size_t size() const = 0;
};

class SciQLopPlotCollection : public QObject, public SciQLopPlotCollectionInterface
{
    Q_OBJECT
    QList<SciQLopPlot*> _plots;

public:
    SciQLopPlotCollection(QObject* parent = nullptr);
    virtual ~SciQLopPlotCollection() = default;
    void addPlot(SciQLopPlot* plot) final;
    void insertPlot(int index, SciQLopPlot* plot) final;
    void removePlot(SciQLopPlot* plot) final;
    void movePlot(int from, int to) final;
    void movePlot(SciQLopPlot* plot, int to) final;
    void clear() final;

    SciQLopPlot* plotAt(int index) final;
    inline const QList<SciQLopPlot*>& plots() const final { return _plots; }

    inline bool contains(SciQLopPlot* plot) const final { return _plots.contains(plot); }

    inline bool empty() const final { return _plots.isEmpty(); }
    inline std::size_t size() const final { return _plots.size(); }

#ifndef BINDINGS_H
    Q_SIGNAL void plotListChanged(const QList<SciQLopPlot*>& plots);
#endif // BINDINGS_H
};

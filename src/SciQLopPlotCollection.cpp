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

#include "SciQLopPlots/SciQLopPlot.hpp"

#include "SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp"

SciQLopPlotCollection::SciQLopPlotCollection(QObject* parent) : QObject(parent) { }

void SciQLopPlotCollection::addPlot(SciQLopPlot* plot)
{
    _plots.append(plot);
    emit plotListChanged(_plots);
}

void SciQLopPlotCollection::insertPlot(int index, SciQLopPlot* plot)
{
    _plots.insert(index, plot);
    emit plotListChanged(_plots);
}

void SciQLopPlotCollection::removePlot(SciQLopPlot* plot)
{
    _plots.removeOne(plot);
    emit plotListChanged(_plots);
}

SciQLopPlot* SciQLopPlotCollection::plotAt(int index)
{
    return _plots.at(index);
}

void SciQLopPlotCollection::clear()
{
    _plots.clear();
    emit plotListChanged(_plots);
}

void SciQLopPlotCollection::movePlot(int from, int to)
{
    _plots.move(from, to);
    emit plotListChanged(_plots);
}

void SciQLopPlotCollection::movePlot(SciQLopPlot* plot, int to)
{
    movePlot(_plots.indexOf(plot), to);
}

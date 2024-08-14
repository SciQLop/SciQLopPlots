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

void SciQLopPlotCollection::addPlot(SciQLopPlotInterface* plot)
{
    insertPlot(_plots.size(), plot);
}

void SciQLopPlotCollection::insertPlot(int index, SciQLopPlotInterface* plot)
{
    _plots.insert(index, plot);
    emit plotListChanged(_plots);
}

void SciQLopPlotCollection::removePlot(SciQLopPlotInterface* plot)
{
    _plots.removeOne(plot);
    emit plotListChanged(_plots);
}

SciQLopPlotInterface* SciQLopPlotCollection::plotAt(int index) const
{
    return _plots.at(index);
}

void SciQLopPlotCollection::set_x_axis_range(double lower, double upper)
{
    for (auto* plot : _plots)
    {
        plot->x_axis()->set_range(lower, upper);
    }
}

void SciQLopPlotCollection::set_time_axis_range(double min, double max)
{
    for (auto* plot : _plots)
    {
        plot->time_axis()->set_range(min, max);
    }
}

void SciQLopPlotCollection::registerBehavior(SciQLopPlotCollectionBehavior* behavior)
{
    behavior->setParent(this);
    _behaviors[behavior->metaObject()->className()] = behavior;
    behavior->updatePlotList(_plots);
    connect(this, &SciQLopPlotCollection::plotListChanged, behavior,
        &SciQLopPlotCollectionBehavior::updatePlotList);
}

void SciQLopPlotCollection::removeBehavior(const QString& type_name)
{
    if (_behaviors.contains(type_name))
    {
        disconnect(this, &SciQLopPlotCollection::plotListChanged, _behaviors[type_name],
            &SciQLopPlotCollectionBehavior::updatePlotList);
        delete _behaviors[type_name];
        _behaviors.remove(type_name);
    }
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

void SciQLopPlotCollection::movePlot(SciQLopPlotInterface* plot, int to)
{
    movePlot(_plots.indexOf(plot), to);
}

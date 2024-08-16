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

void SciQLopPlotCollection::add_plot(SciQLopPlotInterface* plot)
{
    insert_plot(_plots.size(), plot);
}

void SciQLopPlotCollection::insert_plot(int index, SciQLopPlotInterface* plot)
{
    _plots.insert(index, plot);
    emit plotListChanged(_plots);
}

void SciQLopPlotCollection::remove_plot(SciQLopPlotInterface* plot)
{
    _plots.removeOne(plot);
    emit plotListChanged(_plots);
}

SciQLopPlotInterface* SciQLopPlotCollection::plot_at(int index) const
{
    return _plots.at(index);
}

void SciQLopPlotCollection::set_x_axis_range(const SciQLopPlotRange& range)
{
    for (auto* plot : _plots)
    {
        plot->x_axis()->set_range(range);
    }
}

void SciQLopPlotCollection::set_time_axis_range(const SciQLopPlotRange& range)
{
    for (auto* plot : _plots)
    {
        plot->time_axis()->set_range(range);
    }
}

void SciQLopPlotCollection::register_behavior(SciQLopPlotCollectionBehavior* behavior)
{
    behavior->setParent(this);
    _behaviors[behavior->metaObject()->className()] = behavior;
    behavior->updatePlotList(_plots);
    connect(this, &SciQLopPlotCollection::plotListChanged, behavior,
        &SciQLopPlotCollectionBehavior::updatePlotList);
}

void SciQLopPlotCollection::remove_behavior(const QString& type_name)
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

int SciQLopPlotCollection::index(SciQLopPlotInterface* plot) const
{
    return _plots.indexOf(plot);
}

void SciQLopPlotCollection::move_plot(int from, int to)
{
    _plots.move(from, to);
    emit plotListChanged(_plots);
}

void SciQLopPlotCollection::move_plot(SciQLopPlotInterface* plot, int to)
{
    move_plot(_plots.indexOf(plot), to);
}

void SciQLopPlotCollectionInterface::set_x_axis_range(const SciQLopPlotRange& range) { }

void SciQLopPlotCollectionInterface::set_time_axis_range(const SciQLopPlotRange& range) { }

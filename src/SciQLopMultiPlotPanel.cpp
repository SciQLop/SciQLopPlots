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

#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp"


SciQLopMultiPlotPanel::SciQLopMultiPlotPanel(QWidget* parent) : QScrollArea { nullptr }
{
    _container = new SciQLopPlotContainer(this);
    connect(_container, &SciQLopPlotContainer::plotListChanged, this,
        &SciQLopMultiPlotPanel::plotListChanged);
    setWidget(_container);
    this->setWidgetResizable(true);
}

void SciQLopMultiPlotPanel::addPlot(SciQLopPlot* plot)
{
    _container->addPlot(plot);
}

void SciQLopMultiPlotPanel::removePlot(SciQLopPlot* plot)
{
    _container->removePlot(plot);
}

SciQLopPlot* SciQLopMultiPlotPanel::plotAt(int index)
{
    return _container->plotAt(index);
}

const QList<SciQLopPlot*>& SciQLopMultiPlotPanel::plots() const
{
    return _container->plots();
}

void SciQLopMultiPlotPanel::insertPlot(int index, SciQLopPlot* plot)
{
    _container->insertPlot(index, plot);
}

void SciQLopMultiPlotPanel::movePlot(int from, int to)
{
    _container->movePlot(from, to);
}

void SciQLopMultiPlotPanel::movePlot(SciQLopPlot* plot, int to)
{
    _container->movePlot(plot, to);
}

void SciQLopMultiPlotPanel::clear()
{
    _container->clear();
}

bool SciQLopMultiPlotPanel::contains(SciQLopPlot* plot) const
{
    return _container->contains(plot);
}

bool SciQLopMultiPlotPanel::empty() const
{
    return _container->empty();
}

std::size_t SciQLopMultiPlotPanel::size() const
{
    return _container->size();
}

void SciQLopMultiPlotPanel::addWidget(QWidget* widget)
{
    _container->addWidget(widget);
}

void SciQLopMultiPlotPanel::removeWidget(QWidget* widget)
{
    _container->removeWidget(widget, true);
}

SciQLopPlot* SciQLopMultiPlotPanel::createPlot(int index)
{
    auto plot = new SciQLopPlot(this);
    if (index == -1)
        addPlot(plot);
    else
        insertPlot(index, plot);
    return plot;
}

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

#include "SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp"

SciQLopPlotContainer::SciQLopPlotContainer(QWidget* parent) : QSplitter(Qt::Vertical, parent)
{
    this->setLayout(new QVBoxLayout(this));
    this->setContentsMargins(0, 0, 0, 0);
    this->layout()->setContentsMargins(0, 0, 0, 0);
    this->layout()->setSpacing(0);
    this->setChildrenCollapsible(false);
    this->setProperty("empty", true);
}

void SciQLopPlotContainer::insertWidget(int index, QWidget* widget)
{
    QSplitter::insertWidget(index, widget);
    if (auto* plot = qobject_cast<SciQLopPlot*>(widget); plot)
    {
        _plots.insert(index, plot);
        setProperty("empty", false);
        emit plotAdded(plot);
        emit plotListChanged();
    }
}

void SciQLopPlotContainer::addWidget(QWidget* widget)
{
    insertWidget(count(), widget);
}

void SciQLopPlotContainer::insertPlot(int index, SciQLopPlot* plot)
{
    insertWidget(index, plot);
}

void SciQLopPlotContainer::addPlot(SciQLopPlot* plot)
{
    addWidget(plot);
}

void SciQLopPlotContainer::removePlot(SciQLopPlot* plot, bool destroy)
{
    if (plot and plot->parent() == this)
    {
        plot->setParent(nullptr);
        _plots.removeAll(plot);
        if (destroy)
            plot->deleteLater();
        setProperty("empty", plots().isEmpty());
        emit plotRemoved(plot);
        emit plotListChanged();
    }
}

void SciQLopPlotContainer::setXAxisRange(double lower, double upper)
{
    for (auto* plot : plots())
    {
        if (plot->xAxis->range().lower != lower or plot->xAxis->range().upper != upper)
            plot->xAxis->setRange(lower, upper);
    }
}

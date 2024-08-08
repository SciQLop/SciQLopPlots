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

#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotObject.hpp"

void SciQLopMultiPlotObject::addObject(SciQLopPlotInterface* plot) { }

void SciQLopMultiPlotObject::removeObject(SciQLopPlotInterface* plot) { }

void SciQLopMultiPlotObject::replotAll()
{
    for (auto* plot : m_plots)
    {
        plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

SciQLopMultiPlotObject::SciQLopMultiPlotObject(SciQLopPlotCollectionInterface* parent)
        : QObject { dynamic_cast<QObject*>(parent) }
{
    if (auto qobj = dynamic_cast<QObject*>(parent); qobj)
        connect(dynamic_cast<QObject*>(parent),
            SIGNAL(plotListChanged(const QList<SciQLopPlotInterface*>&)), this,
            SLOT(updatePlotList(const QList<SciQLopPlotInterface*>&)));
    else
        throw std::runtime_error("Invalid parent type");
}

SciQLopMultiPlotObject::~SciQLopMultiPlotObject() { }

void SciQLopMultiPlotObject::updatePlotList(const QList<SciQLopPlotInterface*>& plots)
{
    for (auto* plot : plots)
    {
        if (!m_plots.contains(plot))
        {
            m_plots.append(plot);
            addObject(plot);
        }
    }
    for (auto* plot : m_plots)
    {
        if (!plots.contains(plot))
        {
            m_plots.removeOne(plot);
            removeObject(plot);
        }
    }
}
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/plot.hpp"


SciQLopPlots::LineGraph* SciQLopPlots::PlotWidget::addLineGraph(const QColor& color)
{
    auto lineGraph = new SciQLopPlots::LineGraph { m_plot->addGraph(color), this };
    this->connect(lineGraph, &SciQLopPlots::LineGraph::setdata, this,
        QOverload<int, const std::vector<double>&, const std::vector<double>&>::of(
            &SciQLopPlots::PlotWidget::plot));
    this->connect(this, &SciQLopPlots::PlotWidget::xRangeChanged, lineGraph,
        &SciQLopPlots::LineGraph::xRangeChanged);
    return lineGraph;
}

SciQLopPlots::MultiLineGraph* SciQLopPlots::PlotWidget::addMultiLineGraph(
    const std::vector<QColor>& colors)
{
    std::vector<int> indexes;
    std::transform(std::cbegin(colors), std::cend(colors), std::back_inserter(indexes),
        [this](const auto& color) { return this->m_plot->addGraph(color); });
    auto mlineGraph = new SciQLopPlots::MultiLineGraph { indexes, this };
    this->connect(mlineGraph, &SciQLopPlots::MultiLineGraph::setdata, this,
        QOverload<std::vector<int>, const std::vector<double>&, const std::vector<double>&>::of(
            &SciQLopPlots::PlotWidget::plot));
    this->connect(this, &SciQLopPlots::PlotWidget::xRangeChanged, mlineGraph,
        &SciQLopPlots::MultiLineGraph::xRangeChanged);
    return mlineGraph;
}

SciQLopPlots::ColorMapGraph* SciQLopPlots::PlotWidget::addColorMapGraph()
{
    return nullptr;
}

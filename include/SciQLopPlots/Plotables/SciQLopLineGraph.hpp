/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-multigraph.h>

class SciQLopLineGraph : public SciQLopMultiGraphBase
{
    Q_OBJECT
protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) override
    {
        return new QCPMultiGraph(keyAxis, valueAxis);
    }

public:
    explicit SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                              SciQLopPlotAxis* value_axis,
                              const QStringList& labels = QStringList(),
                              QVariantMap metaData = {});
    ~SciQLopLineGraph() override = default;
};

class SciQLopLineGraphFunction : public SciQLopLineGraph,
                                 public SciQLopFunctionGraph
{
    Q_OBJECT
public:
    explicit SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                      SciQLopPlotAxis* value_axis, GetDataPyCallable&& callable,
                                      const QStringList& labels, QVariantMap metaData = {});
    ~SciQLopLineGraphFunction() override = default;
};

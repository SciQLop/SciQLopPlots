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

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

class SciQLopLineGraphRemote : public SciQLopLineGraph,
                               public SciQLopRemoteGraph
{
    Q_OBJECT
public:
    explicit SciQLopLineGraphRemote(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                    SciQLopPlotAxis* value_axis,
                                    const QStringList& labels = QStringList(),
                                    QVariantMap metaData = {});
    ~SciQLopLineGraphRemote() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }

    // busy() reports the remote request/response lifecycle via the mixin flag so
    // it is valid even before QCP components exist (first set_data). set_busy
    // also forwards to the base so the component's own busy flag stays consistent
    // once components exist (a no-op before that).
    inline bool busy() const noexcept override { return remote_busy(); }
    inline void set_busy(bool busy) noexcept override
    {
        set_remote_busy(busy);
        SciQLopLineGraph::set_busy(busy);
        Q_EMIT busy_changed(busy);
    }
};

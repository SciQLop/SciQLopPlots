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

#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-graph2.h>
#include <datasource/abstract-datasource.h>
#include <memory>

class SciQLopSingleLineGraph : public SQPQCPAbstractPlottableWrapper
{
    QCPGraph2* _graph = nullptr;

    struct DataHolder
    {
        PyBuffer x, y;
        std::shared_ptr<QCPAbstractDataSource> source;
    };
    std::shared_ptr<DataHolder> _dataHolder;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;

    Q_OBJECT

    void clear_graph(bool graph_already_removed = false);

public:
    explicit SciQLopSingleLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                    SciQLopPlotAxis* value_axis,
                                    const QStringList& labels = QStringList(),
                                    QVariantMap metaData = {});

    virtual ~SciQLopSingleLineGraph() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline std::size_t line_count() const noexcept { return _graph ? 1 : 0; }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

    void create_graph(const QStringList& labels);
};

class SciQLopSingleLineGraphFunction : public SciQLopSingleLineGraph,
                                       public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopSingleLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                            SciQLopPlotAxis* value_axis,
                                            GetDataPyCallable&& callable,
                                            const QStringList& labels, QVariantMap metaData = {});

    virtual ~SciQLopSingleLineGraphFunction() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

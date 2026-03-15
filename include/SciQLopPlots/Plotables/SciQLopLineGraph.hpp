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

#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-multigraph.h>
#include <span>

class SciQLopLineGraph : public SQPQCPAbstractPlottableWrapper
{
    QCPMultiGraph* _multiGraph = nullptr;
    PyBuffer _x, _y;
    QStringList _pendingLabels;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    bool _got_first_data = false;

    Q_OBJECT

    void clear_graphs(bool graph_already_removed = false);

public:
    Q_ENUMS(FractionStyle)
    explicit SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                              SciQLopPlotAxis* value_axis,
                              const QStringList& labels = QStringList(), QVariantMap metaData = {});

    SciQLopLineGraph(QCustomPlot* parent);

    virtual ~SciQLopLineGraph() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline std::size_t line_count() const noexcept
    {
        return _multiGraph ? _multiGraph->componentCount() : 0;
    }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

    void create_graphs(const QStringList& labels);
};

class SciQLopLineGraphFunction : public SciQLopLineGraph,
                                 public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                      SciQLopPlotAxis* value_axis, GetDataPyCallable&& callable,
                                      const QStringList& labels, QVariantMap metaData = {});

    virtual ~SciQLopLineGraphFunction() override = default;
};

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

#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <qcustomplot.h>
struct LineGraphResampler;
class QThread;

class SciQLopLineGraph : public SQPQCPAbstractPlottableWrapper
{
    LineGraphResampler* _resampler = nullptr;
    QThread* _resampler_thread = nullptr;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    bool _got_first_data = false;

    Q_OBJECT

    // inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    void _setGraphData(QList<QVector<QCPGraphData>> data);


    void clear_graphs(bool graph_already_removed = false);
    void clear_resampler();
    void create_resampler(const QStringList& labels);

    inline const QList<QCPGraph*> lines() const noexcept
    {
        QList<QCPGraph*> graphs;
        for (auto plottable : m_components)
            graphs.append(qobject_cast<QCPGraph*>(plottable->plottable()));
        return graphs;
    }

    inline QCPGraph* line(std::size_t index) const
    {
        if (index < plottable_count())
            return qobject_cast<QCPGraph*>(m_components[index]->plottable());
        return nullptr;
    }

public:
    Q_ENUMS(FractionStyle)
    explicit SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                              SciQLopPlotAxis* value_axis,
                              const QStringList& labels = QStringList(),QVariantMap metaData={});

    SciQLopLineGraph(QCustomPlot* parent);

    virtual ~SciQLopLineGraph() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline std::size_t line_count() const noexcept { return plottable_count(); }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }

    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

private:

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void _setGraphDataSig(QList<QVector<QCPGraphData>> data);
    void create_graphs(const QStringList& labels);
};

class SciQLopLineGraphFunction : public SciQLopLineGraph,
                                 public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                      SciQLopPlotAxis* value_axis, GetDataPyCallable&& callable,
                                      const QStringList& labels,QVariantMap metaData={});

    virtual ~SciQLopLineGraphFunction() override = default;
};

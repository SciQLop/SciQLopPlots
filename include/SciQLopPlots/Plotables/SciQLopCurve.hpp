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
#include "SciQLopLineGraph.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/enums.hpp"
#include <qcustomplot.h>
class QThread;
struct CurveResampler;

class SciQLopCurve : public SQPQCPAbstractPlottableWrapper
{
    CurveResampler* _resampler = nullptr;
    QThread* _resampler_thread = nullptr;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;

    Q_OBJECT

    // inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    void _range_changed(const QCPRange& newRange, const QCPRange& oldRange);

    void _setCurveData(QList<QVector<QCPCurveData>> data);

    void clear_curves(bool curve_already_removed = false);
    void clear_resampler();
    void create_resampler(const QStringList& labels);
    void create_graphs(const QStringList& labels);

    inline void create_curves(const QStringList& labels) { create_graphs(labels); }

    inline QCPCurve* line(std::size_t index) const
    {
        if (index < plottable_count())
            return dynamic_cast<QCPCurve*>(m_components[index]->plottable());
        return nullptr;
    }

    inline const QList<QCPCurve*> lines() const
    {
        QList<QCPCurve*> curves;
        for (auto plottable : m_components)
            curves.append(qobject_cast<QCPCurve*>(plottable->plottable()));
        return curves;
    }

public:
    Q_ENUMS(FractionStyle)
    explicit SciQLopCurve(QCustomPlot* parent, SciQLopPlotAxis* keyAxis, SciQLopPlotAxis* valueAxis,
                          const QStringList& labels, QVariantMap metaData={});

    explicit SciQLopCurve(QCustomPlot* parent, SciQLopPlotAxis* keyAxis,
                          SciQLopPlotAxis* valueAxis, QVariantMap metaData={});

    virtual ~SciQLopCurve() override;

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
    Q_SIGNAL void _setCurveDataSig(QList<QVector<QCPCurveData>> data);
};

class SciQLopCurveFunction : public SciQLopCurve, public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopCurveFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                  SciQLopPlotAxis* value_axis, GetDataPyCallable&& callable,
                                  const QStringList& labels,QVariantMap metaData={});

    virtual ~SciQLopCurveFunction() override = default;
};

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


#include "../Python/BufferProtocol.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopGraph.hpp"
#include <qcustomplot.h>
class QThread;
struct CurveResampler;

class SciQLopCurve : public SQPQCPAbstractPlottableWrapper
{
    CurveResampler* _resampler = nullptr;
    QThread* _resampler_thread = nullptr;

    QCPAxis* _keyAxis;
    QCPAxis* _valueAxis;

    Q_OBJECT

    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }


    void _range_changed(const QCPRange& newRange, const QCPRange& oldRange);

    void _setCurveData(std::size_t index, QVector<QCPCurveData> data);

#ifndef BINDINGS_H
    Q_SIGNAL void _setCurveDataSig(std::size_t index, QVector<QCPCurveData> data);
#endif // !BINDINGS_H

    void clear_curves(bool curve_already_removed = false);
    void clear_resampler();
    void create_resampler(const QStringList& labels);

public:
    Q_ENUMS(FractionStyle)
    explicit SciQLopCurve(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
        const QStringList& labels,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    explicit SciQLopCurve(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    virtual ~SciQLopCurve() override;

    virtual void set_data(Array_view x, Array_view y) override;

    inline QCPCurve* graphAt(std::size_t index) const
    {
        if (index < plottable_count())
            return dynamic_cast<QCPCurve*>(m_plottables[index]);
        return nullptr;
    }
    inline QCPCurve* curveAt(std::size_t index) const { return graphAt(index); }
    inline const QList<QCPCurve*> curves() const
    {
        QList<QCPCurve*> curves;
        for (auto plottable : m_plottables)
            curves.append(qobject_cast<QCPCurve*>(plottable));
        return curves;
    }
    void create_graphs(const QStringList& labels);
    inline void create_curves(const QStringList& labels) { create_graphs(labels); }

    inline std::size_t line_count() const noexcept { return plottable_count(); }

private:
    SciQLopGraph::DataOrder _dataOrder = SciQLopGraph::DataOrder::xFirst;
};

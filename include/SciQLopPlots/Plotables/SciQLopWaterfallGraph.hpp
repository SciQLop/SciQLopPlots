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

#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/enums.hpp"
#include <plottables/plottable-waterfall.h>

class SciQLopWaterfallGraph : public SciQLopMultiGraphBase
{
    Q_OBJECT
protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) override
    {
        return new QCPWaterfallGraph(keyAxis, valueAxis);
    }

public:
    QCPWaterfallGraph* waterfall_graph() const noexcept
    {
        return static_cast<QCPWaterfallGraph*>(_multiGraph);
    }

    explicit SciQLopWaterfallGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                   SciQLopPlotAxis* value_axis,
                                   const QStringList& labels = QStringList(),
                                   QVariantMap metaData = {});
    ~SciQLopWaterfallGraph() override = default;

    Q_SLOT void set_offset_mode(WaterfallOffsetMode mode);
    Q_SLOT void set_uniform_spacing(double spacing);
    Q_SLOT void set_offsets(const QVector<double>& offsets);
    Q_SLOT void set_normalize(bool enabled);
    Q_SLOT void set_gain(double gain);

    WaterfallOffsetMode offset_mode() const;
    double uniform_spacing() const;
    QVector<double> offsets() const;
    bool normalize() const;
    double gain() const;

    double raw_value_at(int component, double key) const;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void offset_mode_changed(WaterfallOffsetMode);
    Q_SIGNAL void uniform_spacing_changed(double);
    Q_SIGNAL void offsets_changed(QVector<double>);
    Q_SIGNAL void normalize_changed(bool);
    Q_SIGNAL void gain_changed(double);
};

class SciQLopWaterfallGraphFunction : public SciQLopWaterfallGraph,
                                       public SciQLopFunctionGraph
{
    Q_OBJECT
public:
    explicit SciQLopWaterfallGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                           SciQLopPlotAxis* value_axis,
                                           GetDataPyCallable&& callable,
                                           const QStringList& labels = QStringList(),
                                           QVariantMap metaData = {});
    ~SciQLopWaterfallGraphFunction() override = default;
};

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
#include "SciQLopPlots/Plotables/SciQLopLineGraph.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/qcp_enums.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

SciQLopLineGraph::SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                   SciQLopPlotAxis* value_axis, const QStringList& labels,
                                   QVariantMap metaData)
    : SciQLopMultiGraphBase("Line", parent, key_axis, value_axis, labels, metaData)
{
    create_graphs(labels);
}

SciQLopLineGraph::~SciQLopLineGraph()
{
    // Still listed among the plot's plottables here, so let it look again once we are gone.
    if (_color_values)
        notify_color_scale_later();
}

void SciQLopLineGraph::set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
{
    SciQLopMultiGraphBase::set_data(std::move(x), std::move(y));
    if (!_color_values)
        return;
    // Same size rule as QCPMultiGraph::setDataSource, which drops its copy on its own.
    if (_color_values->size() != _x.flat_size())
    {
        _color_values.reset();
        notify_color_scale();
        return;
    }
    // Staging a new source drops the values parked for the previous staged one:
    // hand the kept values over again (same buffer, no copy).
    if (_multiGraph)
        _multiGraph->setColorValues(_color_values);
}

void SciQLopLineGraph::set_visible(bool visible) noexcept
{
    SciQLopMultiGraphBase::set_visible(visible);
    notify_color_scale();
}

void SciQLopLineGraph::set_color_data(SciQLopPyBuffer values, ::ColorGradient gradient)
{
    const bool colouring = values.is_valid() && values.flat_size() > 0;
    const std::size_t samples = _x.is_valid() ? _x.flat_size() : 0;
    if (colouring && values.flat_size() != samples)
        throw std::invalid_argument(
            "LineGraph.set_color_data: expected one colour value per x sample ("
            + std::to_string(samples) + "), got " + std::to_string(values.flat_size()));

    _color_values = colouring ? std::make_shared<const std::vector<double>>(
                                    to_double_vector<std::vector<double>>(values))
                              : nullptr;
    _color_gradient = QCPColorGradient(to_qcp(gradient));
    if (_multiGraph)
    {
        if (_color_values)
            _multiGraph->setColorValues(_color_values);
        else
            _multiGraph->clearColorValues();
    }
    push_color_mapping();
    notify_color_scale(colouring ? std::optional { gradient } : std::nullopt);
}

std::optional<std::pair<double, double>> SciQLopLineGraph::color_range(bool log) const
{
    if (!_color_values)
        return std::nullopt;
    double lo = std::numeric_limits<double>::infinity();
    double hi = -lo;
    for (const double v : *_color_values)
        if (std::isfinite(v) && (!log || v > 0))
        {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    if (lo > hi)
        return std::nullopt;
    return std::pair { lo, hi };
}

void SciQLopLineGraph::attach_color_scale(QCPColorScale* scale)
{
    if (_color_scale == scale)
        return;
    if (_color_scale)
        _color_scale->disconnect(this);
    _color_scale = scale;
    if (scale)
    {
        connect(scale, &QCPColorScale::gradientChanged, this, &SciQLopLineGraph::push_color_mapping);
        connect(scale, &QCPColorScale::dataRangeChanged, this, &SciQLopLineGraph::push_color_mapping);
        connect(scale, &QCPColorScale::dataScaleTypeChanged, this,
                &SciQLopLineGraph::push_color_mapping);
    }
    push_color_mapping();
}

namespace
{
//! The mapper needs a non-empty range: a constant scalar gets one unit around it.
QCPRange own_color_range(const std::optional<std::pair<double, double>>& range)
{
    if (!range)
        return QCPRange(0.0, 1.0);
    const auto [lo, hi] = *range;
    return lo < hi ? QCPRange(lo, hi) : QCPRange(lo - 0.5, hi + 0.5);
}
}

void SciQLopLineGraph::push_color_mapping()
{
    if (!_multiGraph)
        return;
    if (_color_scale)
    {
        _multiGraph->setColorGradient(_color_scale->gradient());
        _multiGraph->setColorRange(_color_scale->dataRange());
        _multiGraph->setColorScaleType(_color_scale->dataScaleType());
    }
    else
    {
        _multiGraph->setColorGradient(_color_gradient);
        _multiGraph->setColorRange(own_color_range(color_range(false)));
        _multiGraph->setColorScaleType(QCPAxis::stLinear);
    }
    // The multigraph's colour setters do not repaint.
    Q_EMIT this->replot();
}

SciQLopLineGraphFunction::SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                                   SciQLopPlotAxis* value_axis,
                                                   GetDataPyCallable&& callable,
                                                   const QStringList& labels,
                                                   QVariantMap metaData)
    : SciQLopLineGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

SciQLopLineGraphRemote::SciQLopLineGraphRemote(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                               SciQLopPlotAxis* value_axis,
                                               const QStringList& labels,
                                               QVariantMap metaData)
    : SciQLopLineGraph{parent, key_axis, value_axis, labels, std::move(metaData)}
    , SciQLopRemoteGraph(this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

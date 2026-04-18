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
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"
#include <algorithm>
#include <cmath>

SciQLopWaterfallGraph::SciQLopWaterfallGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                             SciQLopPlotAxis* value_axis,
                                             const QStringList& labels, QVariantMap metaData)
    : SciQLopMultiGraphBase("Waterfall", parent, key_axis, value_axis, labels, metaData)
{
    create_graphs(labels);
}

static QCPWaterfallGraph::OffsetMode to_qcp(WaterfallOffsetMode mode)
{
    return mode == WaterfallOffsetMode::Uniform
               ? QCPWaterfallGraph::omUniform
               : QCPWaterfallGraph::omCustom;
}

static WaterfallOffsetMode from_qcp(QCPWaterfallGraph::OffsetMode mode)
{
    return mode == QCPWaterfallGraph::omUniform
               ? WaterfallOffsetMode::Uniform
               : WaterfallOffsetMode::Custom;
}

void SciQLopWaterfallGraph::set_offset_mode(WaterfallOffsetMode mode)
{
    if (auto* w = waterfall_graph())
    {
        if (from_qcp(w->offsetMode()) == mode)
            return;
        w->setOffsetMode(to_qcp(mode));
        Q_EMIT offset_mode_changed(mode);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_uniform_spacing(double spacing)
{
    if (auto* w = waterfall_graph())
    {
        if (w->uniformSpacing() == spacing)
            return;
        w->setUniformSpacing(spacing);
        Q_EMIT uniform_spacing_changed(spacing);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_offsets(const QVector<double>& offsets)
{
    if (auto* w = waterfall_graph())
    {
        const auto n = static_cast<int>(line_count());
        if (n > 0 && offsets.size() != n)
            throw std::invalid_argument(
                "offsets length must match number of traces");
        if (w->offsets() == offsets)
            return;
        w->setOffsets(offsets);
        Q_EMIT offsets_changed(offsets);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_normalize(bool enabled)
{
    if (auto* w = waterfall_graph())
    {
        if (w->normalize() == enabled)
            return;
        w->setNormalize(enabled);
        Q_EMIT normalize_changed(enabled);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_gain(double gain)
{
    if (auto* w = waterfall_graph())
    {
        if (w->gain() == gain)
            return;
        w->setGain(gain);
        Q_EMIT gain_changed(gain);
        Q_EMIT replot();
    }
}

WaterfallOffsetMode SciQLopWaterfallGraph::offset_mode() const
{
    return waterfall_graph() ? from_qcp(waterfall_graph()->offsetMode())
                              : WaterfallOffsetMode::Uniform;
}

double SciQLopWaterfallGraph::uniform_spacing() const
{
    return waterfall_graph() ? waterfall_graph()->uniformSpacing() : 1.0;
}

QVector<double> SciQLopWaterfallGraph::offsets() const
{
    return waterfall_graph() ? waterfall_graph()->offsets() : QVector<double>{};
}

bool SciQLopWaterfallGraph::normalize() const
{
    return waterfall_graph() ? waterfall_graph()->normalize() : true;
}

double SciQLopWaterfallGraph::gain() const
{
    return waterfall_graph() ? waterfall_graph()->gain() : 1.0;
}

SciQLopWaterfallGraphFunction::SciQLopWaterfallGraphFunction(QCustomPlot* parent,
                                                             SciQLopPlotAxis* key_axis,
                                                             SciQLopPlotAxis* value_axis,
                                                             GetDataPyCallable&& callable,
                                                             const QStringList& labels,
                                                             QVariantMap metaData)
    : SciQLopWaterfallGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

double SciQLopWaterfallGraph::raw_value_at(int component, double key) const
{
    if (!_x.is_valid() || !_y.is_valid())
        return std::nan("");

    const auto* keys = static_cast<const double*>(_x.raw_data());
    const int n = static_cast<int>(_x.flat_size());
    if (n == 0 || component < 0)
        return std::nan("");

    auto it = std::lower_bound(keys, keys + n, key);
    int idx;
    if (it == keys + n) idx = n - 1;
    else if (it == keys) idx = 0;
    else
    {
        idx = static_cast<int>(it - keys);
        if (idx > 0 && std::abs(*(it - 1) - key) < std::abs(*it - key))
            idx = idx - 1;
    }

    double result = std::nan("");
    dispatch_dtype(_y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(_y.raw_data());
        if (_y.ndim() == 1)
        {
            if (component == 0) result = static_cast<double>(values[idx]);
        }
        else
        {
            const auto n_cols = static_cast<int>(_y.size(1));
            if (component >= n_cols) return;
            if (_y.row_major())
                result = static_cast<double>(values[idx * n_cols + component]);
            else
                result = static_cast<double>(values[component * n + idx]);
        }
    });
    return result;
}

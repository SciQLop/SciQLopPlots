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
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Plotables/SciQLopSingleLineGraph.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/Python/Validation.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include <datasource/soa-datasource.h>
#include <colorgradient.h>
#include <algorithm>
#include <cmath>
#include <limits>

void SciQLopSingleLineGraph::create_graph(const QStringList& labels)
{
    if (_graph)
        clear_graph();

    _graph = new QCPGraph2(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    if (!labels.isEmpty())
        _graph->setName(labels.first());

    auto component = new SciQLopGraphComponent(_graph, this);
    if (!labels.isEmpty())
        component->set_name(labels.first());
    _register_component(component);
    connect(_graph, &QCPAbstractPlottable::busyChanged,
            this, &SciQLopPlottableInterface::busy_changed);
}

void SciQLopSingleLineGraph::clear_graph(bool graph_already_removed)
{
    // clear_plottables() already deletes the SciQLopGraphComponent wrapping
    // _graph, and its destructor removes/deletes the underlying plottable
    // itself — re-touching _graph afterwards would be a use-after-free.
    Q_UNUSED(graph_already_removed);
    clear_plottables();
    _graph = nullptr;
}

SciQLopSingleLineGraph::SciQLopSingleLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                                SciQLopPlotAxis* value_axis,
                                                const QStringList& labels, QVariantMap metaData)
    : SQPQCPAbstractPlottableWrapper("Line", metaData, parent)
    , _keyAxis{key_axis}
    , _valueAxis{value_axis}
{
    create_graph(labels);
}

SciQLopSingleLineGraph::~SciQLopSingleLineGraph()
{
    clear_graph();
}

void SciQLopSingleLineGraph::set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
{
    if (!_graph || !x.is_valid() || !y.is_valid())
        return;

    if (x.format_code() != 'd')
        throw std::runtime_error("Keys (x) must be float64");
    // The y span below is built with x's length: a shorter y would be read out
    // of bounds at render time. y may be 1D or a single (n, 1) column.
    sqp::validation::validate_xy(x, y);
    if (y.flat_size() != x.flat_size())
        throw std::invalid_argument("y must hold exactly one value per x sample");

    const auto* keys = static_cast<const double*>(x.raw_data());
    const int n = static_cast<int>(x.flat_size());

    if (n > 0)
        m_data_range = SciQLopPlotRange(keys[0], keys[n - 1]);
    else
        m_data_range = SciQLopPlotRange();

    dispatch_dtype(y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(y.raw_data());

        auto source = std::make_shared<QCPSoADataSource<
            std::span<const double>, std::span<const V>>>(
            std::span<const double>(keys, n), std::span<const V>(values, n));

        _dataHolder = std::make_shared<DataHolder>(DataHolder{x, y, source});
        // Aliasing shared_ptr: points to source but co-owns _dataHolder,
        // so PyBuffers stay alive as long as the async pipeline holds a reference.
        auto aliased = std::shared_ptr<QCPAbstractDataSource>(_dataHolder, source.get());
        _graph->setDataSource(std::move(aliased));
    });

    Q_EMIT this->replot();
    check_first_data(n);
    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

void SciQLopSingleLineGraph::set_color_data(SciQLopPyBuffer values, ::ColorGradient gradient)
{
    if (!_graph || !values.is_valid())
        return;

    const int n = static_cast<int>(values.flat_size());
    if (n == 0)
        return;

    std::vector<float> normalized(n);

    dispatch_dtype(values.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* src = static_cast<const V*>(values.raw_data());

        V vmin = std::numeric_limits<V>::max();
        V vmax = std::numeric_limits<V>::lowest();
        for (int i = 0; i < n; ++i)
        {
            if (std::isfinite(static_cast<double>(src[i])))
            {
                if (src[i] < vmin) vmin = src[i];
                if (src[i] > vmax) vmax = src[i];
            }
        }

        const double range = (vmax > vmin) ? static_cast<double>(vmax - vmin) : 1.0;
        for (int i = 0; i < n; ++i)
        {
            if (std::isfinite(static_cast<double>(src[i])))
                normalized[i] = static_cast<float>((static_cast<double>(src[i]) - vmin) / range);
            else
                normalized[i] = 0.0f;
        }
    });

    _graph->setScatterColorValues(std::move(normalized));
    _graph->setScatterColorGradient(QCPColorGradient(to_qcp(gradient)));
    Q_EMIT this->replot();
}

QList<SciQLopPyBuffer> SciQLopSingleLineGraph::data() const noexcept
{
    if (_dataHolder)
        return {_dataHolder->x, _dataHolder->y};
    return {};
}

void SciQLopSingleLineGraph::collect_visible_values(const SciQLopPlotRange& visible_key_range,
                                                    std::vector<double>& out) const noexcept
{
    if (!_dataHolder)
        return;
    const auto& x = _dataHolder->x;
    const auto& y = _dataHolder->y;
    if (!x.is_valid() || !y.is_valid())
        return;
    if (x.format_code() != 'd')
        return;
    const std::size_t n = x.flat_size();
    if (n == 0 || y.flat_size() < n)
        return;

    const double x_lo = visible_key_range.first;
    const double x_hi = visible_key_range.second;
    const double* xs = x.data();

    // x is sorted (NeoQCP source contract): bound the scan to the visible
    // window instead of testing every sample and over-reserving.
    const std::size_t i0 = std::lower_bound(xs, xs + n, x_lo) - xs;
    const std::size_t i1 = std::upper_bound(xs + i0, xs + n, x_hi) - xs;
    if (i0 >= i1)
        return;

    out.reserve(out.size() + (i1 - i0));
    try
    {
        dispatch_dtype(y.format_code(), [&](auto tag) {
            using V = typename decltype(tag)::type;
            const auto* ys = static_cast<const V*>(y.raw_data());
            for (std::size_t i = i0; i < i1; ++i)
            {
                const double v = static_cast<double>(ys[i]);
                if constexpr (std::is_floating_point_v<V>)
                {
                    if (!std::isfinite(v))
                        continue;
                }
                out.push_back(v);
            }
        });
    }
    catch (const std::invalid_argument&) { /* unsupported dtype — skip */ }
}

void SciQLopSingleLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_graph) _graph->setKeyAxis(a); });
}

void SciQLopSingleLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_graph) _graph->setValueAxis(a); });
}

SciQLopSingleLineGraphFunction::SciQLopSingleLineGraphFunction(
    QCustomPlot* parent, SciQLopPlotAxis* key_axis, SciQLopPlotAxis* value_axis,
    GetDataPyCallable&& callable, const QStringList& labels, QVariantMap metaData)
    : SciQLopSingleLineGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

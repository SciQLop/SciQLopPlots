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
#include "SciQLopPlots/Plotables/SciQLopSingleLineGraph.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"

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
}

void SciQLopSingleLineGraph::clear_graph(bool graph_already_removed)
{
    clear_plottables();
    if (_graph && !graph_already_removed)
    {
        auto plot = _graph->parentPlot();
        if (plot)
            plot->removePlottable(_graph);
    }
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

void SciQLopSingleLineGraph::set_data(PyBuffer x, PyBuffer y)
{
    if (!_graph || !x.is_valid() || !y.is_valid())
        return;

    if (x.format_code() != 'd')
        throw std::runtime_error("Keys (x) must be float64");

    _x = x;
    _y = y;

    const auto* keys = static_cast<const double*>(x.raw_data());
    const int n = static_cast<int>(x.flat_size());

    dispatch_dtype(y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(y.raw_data());
        _graph->viewData(
            std::span<const double>(keys, n), std::span<const V>(values, n));
    });

    Q_EMIT this->replot();
    if (!_got_first_data && n > 0)
    {
        _got_first_data = true;
        Q_EMIT request_rescale();
    }
    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

QList<PyBuffer> SciQLopSingleLineGraph::data() const noexcept
{
    return {_x, _y};
}

void SciQLopSingleLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _keyAxis = qcp_axis;
        if (_graph)
            _graph->setKeyAxis(qcp_axis->qcp_axis());
    }
}

void SciQLopSingleLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _valueAxis = qcp_axis;
        if (_graph)
            _graph->setValueAxis(qcp_axis->qcp_axis());
    }
}

SciQLopSingleLineGraphFunction::SciQLopSingleLineGraphFunction(
    QCustomPlot* parent, SciQLopPlotAxis* key_axis, SciQLopPlotAxis* value_axis,
    GetDataPyCallable&& callable, const QStringList& labels, QVariantMap metaData)
    : SciQLopSingleLineGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

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
#include <vector>

void SciQLopLineGraph::create_graphs(const QStringList& labels)
{
    if (_multiGraph)
        clear_graphs();

    _multiGraph = new QCPMultiGraph(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    if (!labels.isEmpty())
        _pendingLabels = labels;
}

void SciQLopLineGraph::clear_graphs(bool graph_already_removed)
{
    clear_plottables();
    if (_multiGraph && !graph_already_removed)
    {
        auto plot = _multiGraph->parentPlot();
        if (plot)
            plot->removePlottable(_multiGraph);
    }
    _multiGraph = nullptr;
}

SciQLopLineGraph::SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                   SciQLopPlotAxis* value_axis, const QStringList& labels,
                                   QVariantMap metaData)
    : SQPQCPAbstractPlottableWrapper("Line", metaData, parent)
    , _keyAxis{key_axis}
    , _valueAxis{value_axis}
{
    create_graphs(labels);
}

SciQLopLineGraph::SciQLopLineGraph(QCustomPlot* parent)
    : SQPQCPAbstractPlottableWrapper("Line", {}, parent)
    , _keyAxis{nullptr}
    , _valueAxis{nullptr}
{
}

SciQLopLineGraph::~SciQLopLineGraph()
{
    clear_graphs();
}

void SciQLopLineGraph::set_data(PyBuffer x, PyBuffer y)
{
    if (!_multiGraph || !x.is_valid() || !y.is_valid())
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

        if (y.ndim() == 1)
        {
            std::vector<std::span<const V>> columns{std::span<const V>(values, n)};
            _multiGraph->viewData<double, V>(std::span<const double>(keys, n), std::move(columns));
        }
        else
        {
            const auto n_cols = y.size(1);
            if (y.row_major())
            {
                std::vector<std::vector<V>> owned_columns(n_cols);
                for (std::size_t col = 0; col < n_cols; ++col)
                {
                    owned_columns[col].resize(n);
                    for (int row = 0; row < n; ++row)
                        owned_columns[col][row] = values[row * n_cols + col];
                }
                std::vector<double> owned_keys(keys, keys + n);
                std::vector<std::vector<V>> cols_move;
                cols_move.reserve(n_cols);
                for (auto& col : owned_columns)
                    cols_move.push_back(std::move(col));
                _multiGraph->setData(std::move(owned_keys), std::move(cols_move));
            }
            else
            {
                std::vector<std::span<const V>> columns;
                columns.reserve(n_cols);
                for (std::size_t col = 0; col < n_cols; ++col)
                    columns.emplace_back(values + col * n, n);
                _multiGraph->viewData<double, V>(
                    std::span<const double>(keys, n), std::move(columns));
            }
        }
    });

    // After viewData/setData, syncComponentCount has run — create wrappers if needed
    const int nComponents = _multiGraph->componentCount();
    if (static_cast<int>(plottable_count()) < nComponents)
    {
        if (!_pendingLabels.isEmpty())
            _multiGraph->setComponentNames(_pendingLabels);
        for (int i = static_cast<int>(plottable_count()); i < nComponents; ++i)
        {
            auto component = new SciQLopGraphComponent(_multiGraph, i, this);
            if (i < _pendingLabels.size())
                component->set_name(_pendingLabels.value(i));
            _register_component(component);
        }
        _pendingLabels.clear();
    }

    Q_EMIT this->replot();
    if (!_got_first_data && n > 0)
    {
        _got_first_data = true;
        Q_EMIT request_rescale();
    }
    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

QList<PyBuffer> SciQLopLineGraph::data() const noexcept
{
    return {_x, _y};
}

void SciQLopLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _keyAxis = qcp_axis;
        if (_multiGraph)
            _multiGraph->setKeyAxis(qcp_axis->qcp_axis());
    }
}

void SciQLopLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _valueAxis = qcp_axis;
        if (_multiGraph)
            _multiGraph->setValueAxis(qcp_axis->qcp_axis());
    }
}

SciQLopLineGraphFunction::SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                                   SciQLopPlotAxis* value_axis,
                                                   GetDataPyCallable&& callable,
                                                   const QStringList& labels, QVariantMap metaData)
    : SciQLopLineGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

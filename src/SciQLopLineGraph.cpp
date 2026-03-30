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
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Plotables/SciQLopLineGraph.hpp"
#include <datasource/row-major-multi-datasource.h>
#include <datasource/soa-multi-datasource.h>
#include <vector>

void SciQLopLineGraph::create_graphs(const QStringList& labels)
{
    if (_multiGraph)
        clear_graphs();

    _multiGraph = new QCPMultiGraph(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    connect(_multiGraph, &QCPAbstractPlottable::busyChanged,
            this, &SciQLopPlottableInterface::busy_changed);
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

    if (n > 0)
        m_data_range = SciQLopPlotRange(keys[0], keys[n - 1]);
    else
        m_data_range = SciQLopPlotRange();

    // Build the data source with a dataGuard that co-owns the PyBuffers,
    // preventing use-after-free when the async pipeline thread reads while
    // new data arrives and replaces _x/_y.
    struct Buffers { PyBuffer x, y; };
    auto guard = std::make_shared<Buffers>(Buffers{x, y});

    dispatch_dtype(y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(y.raw_data());

        if (y.ndim() == 1)
        {
            std::vector<std::span<const V>> columns{std::span<const V>(values, n)};
            auto source = std::make_shared<
                QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
                std::span<const double>(keys, n), std::move(columns), guard);
            _multiGraph->setDataSource(std::move(source));
        }
        else
        {
            const auto n_cols = y.size(1);
            if (y.row_major())
            {
                auto source = std::make_shared<QCPRowMajorMultiDataSource<double, V>>(
                    std::span<const double>(keys, n),
                    values, n, static_cast<int>(n_cols), static_cast<int>(n_cols), guard);
                _multiGraph->setDataSource(std::move(source));
            }
            else
            {
                std::vector<std::span<const V>> columns;
                columns.reserve(n_cols);
                for (std::size_t col = 0; col < n_cols; ++col)
                    columns.emplace_back(values + col * n, n);
                auto source = std::make_shared<
                    QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
                    std::span<const double>(keys, n), std::move(columns), guard);
                _multiGraph->setDataSource(std::move(source));
            }
        }
    });
    _dataHolder = std::move(guard);

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
    check_first_data(n);
    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

QList<PyBuffer> SciQLopLineGraph::data() const noexcept
{
    return {_x, _y};
}

void SciQLopLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setKeyAxis(a); });
}

void SciQLopLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setValueAxis(a); });
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

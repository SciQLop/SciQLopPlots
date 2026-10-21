/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Plotables/SciQLopHistogram.hpp"
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/constants.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include <datasource/row-major-multi-datasource.h>
#include <datasource/soa-multi-datasource.h>
#include <cmath>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

// The binning ignores keys, but a multi source needs sorted ones: 0..rows-1.
struct BinnedBuffers
{
    SciQLopPyBuffer x, y;
    std::vector<double> keys;
};

std::size_t rows_of(const SciQLopPyBuffer& buffer)
{
    return buffer.ndim() == 1 ? buffer.flat_size() : buffer.size(0);
}

std::size_t columns_of(const SciQLopPyBuffer& buffer)
{
    return buffer.ndim() == 1 ? 1 : buffer.size(1);
}

void check_binnable(const SciQLopPyBuffer& y)
{
    if (y.ndim() < 1 || y.ndim() > 2)
        throw std::invalid_argument("histogram values must be 1-D or 2-D");
}

template <typename V>
std::shared_ptr<QCPAbstractMultiDataSource> make_source(std::shared_ptr<const BinnedBuffers> data)
{
    const auto* values = static_cast<const V*>(data->y.raw_data());
    const int rows = static_cast<int>(data->keys.size());
    const int columns = static_cast<int>(columns_of(data->y));
    const std::span<const double> keys(data->keys);
    if (data->y.ndim() == 2 && data->y.row_major())
        return std::make_shared<QCPRowMajorMultiDataSource<double, V>>(keys, values, rows, columns,
                                                                        columns, data);
    std::vector<std::span<const V>> spans;
    for (int c = 0; c < columns; ++c)
        spans.emplace_back(values + static_cast<std::size_t>(c) * rows, rows);
    return std::make_shared<QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
        keys, std::move(spans), data);
}

std::shared_ptr<const BinnedBuffers> hold(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y)
{
    auto data = std::make_shared<BinnedBuffers>(BinnedBuffers { x, y, {} });
    data->keys.resize(rows_of(y));
    std::iota(data->keys.begin(), data->keys.end(), 0.0);
    return data;
}

std::vector<std::vector<double>> split_columns(const SciQLopPyBuffer& heights)
{
    const auto flat = to_double_vector<std::vector<double>>(heights);
    const std::size_t rows = rows_of(heights), columns = columns_of(heights);
    const bool row_major = heights.ndim() == 2 && heights.row_major();
    std::vector<std::vector<double>> split(columns, std::vector<double>(rows));
    for (std::size_t c = 0; c < columns; ++c)
        for (std::size_t r = 0; r < rows; ++r)
            split[c][r] = row_major ? flat[r * columns + c] : flat[c * rows + r];
    return split;
}

void check_binned(const SciQLopPyBuffer& edges, const SciQLopPyBuffer& heights)
{
    check_binnable(heights);
    if (edges.ndim() != 1 || edges.flat_size() != rows_of(heights) + 1)
        throw std::invalid_argument(
            "bars: edges must be 1-D with one more entry than heights has rows");
}

} // namespace

SciQLopHistogram::SciQLopHistogram(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                   SciQLopPlotAxis* yAxis, const QString& name, int bins,
                                   QVariantMap metaData)
    : SciQLopGraphInterface("Histogram", std::move(metaData), parent)
    , _keyAxis { xAxis }
    , _valueAxis { yAxis }
{
    if (bins <= 0)
        throw std::invalid_argument("histogram bins must be positive");
    _hist = new QCPHistogram(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    _hist->setLayer(Constants::LayersNames::Main);
    _hist->setBins(bins);
    // Not set_name(): an auto-named instance must not count as user-named.
    this->setObjectName(name);
    _hist->setName(name);
    connect(_hist, &QCPHistogram::destroyed, this, &SciQLopHistogram::_hist_got_destroyed);
    connect(_hist, &QCPAbstractPlottable::busyChanged, this,
            &SciQLopPlottableInterface::busy_changed);
    connect(_hist, &QCPHistogram::edgesChanged, this, &SciQLopHistogram::_on_edges_changed);
    connect(_hist, &QCPHistogram::histogramReady, this, &SciQLopHistogram::_on_histogram_ready);
    // SciQLopPlotAxis::set_log blocks QCPAxis::scaleTypeChanged, which QCPHistogram re-bins on.
    connect(_keyAxis, &SciQLopPlotAxis::log_changed, this,
            [this](bool)
            {
                if (_hist)
                    _hist->resetEdges();
            });
    if (auto* legend_item = _legend_item())
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                &SciQLopHistogram::set_selected);
}

SciQLopHistogram::~SciQLopHistogram()
{
    if (!_hist)
        return;
    auto* plot = _plot();
    auto* hist = _hist.data();
    _hist = nullptr;
    if (plot && plot->hasPlottable(hist))
        (void)plot->removePlottable(hist);
}

QCustomPlot* SciQLopHistogram::_plot() const
{
    return qobject_cast<QCustomPlot*>(this->parent());
}

QCPPlottableLegendItem* SciQLopHistogram::_legend_item() const
{
    auto* plot = _plot();
    return plot && _hist ? plot->legend->itemWithPlottable(_hist) : nullptr;
}

void SciQLopHistogram::_hist_got_destroyed()
{
    _hist = nullptr;
}

void SciQLopHistogram::set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
{
    if (!_hist || !y.is_valid())
        return;
    _bin(x, y);
    _buffers = { x, y };
    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

void SciQLopHistogram::set_values(SciQLopPyBuffer values)
{
    if (!_hist || !values.is_valid())
        return;
    _bin(SciQLopPyBuffer(), values);
    _buffers = { values };
    Q_EMIT data_changed();
}

void SciQLopHistogram::_bin(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y)
{
    check_binnable(y);
    auto data = hold(x, y);
    auto source = dispatch_dtype(y.format_code(),
                                 [&](auto tag)
                                 {
                                     using V = typename decltype(tag)::type;
                                     return make_source<V>(data);
                                 });
    _apply_default_style(columns_of(y));
    _hist->setDataSource(std::move(source));
}

void SciQLopHistogram::set_binned(SciQLopPyBuffer edges, SciQLopPyBuffer heights)
{
    if (!_hist)
        return;
    check_binned(edges, heights);
    auto split = split_columns(heights);
    _apply_default_style(split.size());
    _buffers = { edges, heights };
    _hist->setBinned(to_double_vector<std::vector<double>>(edges), std::move(split));
    Q_EMIT data_changed();
}

void SciQLopHistogram::_apply_default_style(std::size_t columns)
{
    if (_style || !_hist)
        return;
    const auto style = to_qcp(columns > 1 ? HistogramStyle::Step : HistogramStyle::Bars);
    if (_hist->style() != style)
        _hist->setStyle(style);
}

void SciQLopHistogram::_on_edges_changed()
{
    _edges_moved = true;
    Q_EMIT bins_changed();
}

// An export bins synchronously without signals, so "edges moved" alone would miss the
// first fit: also rescale on the first result after having none.
void SciQLopHistogram::_on_histogram_ready()
{
    const auto* result = _hist ? _hist->histogram() : nullptr;
    const bool edges_moved = std::exchange(_edges_moved, false);
    const bool first = !_had_result;
    _had_result = result != nullptr;
    if (!result)
        return;
    _apply_default_style(result->heights.size());
    if (first || edges_moved)
        Q_EMIT request_rescale();
}

void SciQLopHistogram::set_bins(int bins)
{
    // QCPHistogram silently ignores non-positive values.
    if (bins <= 0)
        throw std::invalid_argument("histogram bins must be positive");
    if (_hist)
        _hist->setBins(bins);
}

int SciQLopHistogram::bins() const noexcept
{
    return _hist ? _hist->bins() : 0;
}

void SciQLopHistogram::set_bin_range(double lower, double upper)
{
    if (!std::isfinite(lower) || !std::isfinite(upper))
        throw std::invalid_argument("histogram range: bounds must be finite");
    if (!(lower < upper))
        throw std::invalid_argument("histogram range: lower must be below upper");
    if (log_bins() && lower <= 0)
        throw std::invalid_argument("histogram range: log bins need a positive lower bound");
    if (_hist)
        _hist->setFixedRange(QCPRange(lower, upper));
}

void SciQLopHistogram::clear_bin_range()
{
    if (_hist)
        _hist->setFixedRange(std::nullopt);
}

void SciQLopHistogram::reset_edges()
{
    if (_hist)
        _hist->resetEdges();
}

QList<double> SciQLopHistogram::bin_edges() const
{
    const auto* result = _hist ? _hist->histogram() : nullptr;
    if (!result)
        return {};
    return QList<double>(result->edges.begin(), result->edges.end());
}

QList<QList<double>> SciQLopHistogram::bin_heights() const
{
    const auto* result = _hist ? _hist->histogram() : nullptr;
    QList<QList<double>> heights;
    if (!result)
        return heights;
    for (const auto& column : result->heights)
        heights.append(QList<double>(column.begin(), column.end()));
    return heights;
}

void SciQLopHistogram::set_log_bins(bool log)
{
    _keyAxis->set_log(log);
}

bool SciQLopHistogram::log_bins() const noexcept
{
    return _keyAxis && _keyAxis->log();
}

void SciQLopHistogram::set_normalization(HistogramNormalization normalization)
{
    if (_hist)
        _hist->setNormalization(to_qcp(normalization));
}

HistogramNormalization SciQLopHistogram::normalization() const noexcept
{
    return _hist ? from_qcp(_hist->normalization()) : HistogramNormalization::Count;
}

void SciQLopHistogram::set_style(HistogramStyle style)
{
    _style = style;
    if (_hist)
        _hist->setStyle(to_qcp(style));
}

HistogramStyle SciQLopHistogram::style() const noexcept
{
    return _hist ? from_qcp(_hist->style()) : _style.value_or(HistogramStyle::Bars);
}

void SciQLopHistogram::set_name(const QString& name) noexcept
{
    SciQLopGraphInterface::set_name(name);
    if (_hist)
        _hist->setName(name);
}

QString SciQLopHistogram::layer() const noexcept
{
    return _hist && _hist->layer() ? _hist->layer()->name() : QString();
}

void SciQLopHistogram::set_colors(const QList<QColor>& colors)
{
    if (!_hist)
        return;
    _hist->setColumnColors(colors);
    Q_EMIT colors_changed(colors);
}

QList<QColor> SciQLopHistogram::colors() const noexcept
{
    return _hist ? _hist->columnColors() : QList<QColor>();
}

void SciQLopHistogram::set_visible(bool visible) noexcept
{
    if (!_hist || _hist->visible() == visible)
        return;
    _hist->setVisible(visible);
    Q_EMIT visible_changed(visible);
    Q_EMIT replot();
}

bool SciQLopHistogram::visible() const noexcept
{
    return _hist && _hist->visible();
}

void SciQLopHistogram::set_selected(bool selected) noexcept
{
    if (_selected == selected)
        return;
    _selected = selected;
    if (auto* legend_item = _legend_item(); legend_item && legend_item->selected() != selected)
        legend_item->setSelected(selected);
    Q_EMIT selection_changed(selected);
}

bool SciQLopHistogram::busy() const noexcept
{
    return _hist && _hist->busy();
}

void SciQLopHistogram::set_busy(bool busy) noexcept
{
    if (_hist)
        _hist->setBusy(busy);
}

void SciQLopHistogram::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis,
               [this](auto* a)
               {
                   if (_hist)
                       _hist->setKeyAxis(a);
               });
}

void SciQLopHistogram::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis,
               [this](auto* a)
               {
                   if (_hist)
                       _hist->setValueAxis(a);
               });
}

SciQLopHistogramFunction::SciQLopHistogramFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                                   SciQLopPlotAxis* yAxis,
                                                   GetDataPyCallable&& callable,
                                                   const QString& name, int bins)
    : SciQLopHistogram { parent, xAxis, yAxis, name, bins }
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    // No fetch from xAxis here, unlike the line graphs: it holds values, not the fetch window.
    // The factory that picks the observed axis starts the first fetch.
}

SciQLopHistogramRemote::SciQLopHistogramRemote(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                               SciQLopPlotAxis* yAxis, const QString& name,
                                               int bins, QVariantMap metaData)
    : SciQLopHistogram { parent, xAxis, yAxis, name, bins, std::move(metaData) }
    , SciQLopRemoteGraph(this, 2)
{
    this->set_range({ parent->xAxis->range().lower, parent->xAxis->range().upper });
}

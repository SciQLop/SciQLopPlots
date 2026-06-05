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
#include "SciQLopPlots/Plotables/SciQLopHistogram2D.hpp"
#include "SciQLopPlots/PercentileMath.hpp"
#include "SciQLopPlots/constants.hpp"
#include <algorithm>
#include <cmath>
#include <plottables/plottable-colormap.h>
#include <vector>

void SciQLopHistogram2D::_hist_got_destroyed()
{
    _hist = nullptr;
}

SciQLopHistogram2D::SciQLopHistogram2D(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                       SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                       const QString& name, int key_bins, int value_bins,
                                       QVariantMap metaData)
    : SciQLopColorMapBase(xAxis, yAxis, zAxis, std::move(metaData), parent)
{
    _hist = new QCPHistogram2D(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    _hist->setLayer(Constants::LayersNames::ColorMap);
    _hist->setBins(key_bins, value_bins);
    connect(_hist, &QCPHistogram2D::destroyed, this, &SciQLopHistogram2D::_hist_got_destroyed);
    connect(_hist, &QCPAbstractPlottable::busyChanged,
            this, &SciQLopPlottableInterface::busy_changed);
    SciQLopHistogram2D::set_gradient(ColorGradient::Jet);
    SciQLopHistogram2D::set_name(name);

    // The bin scale follows the axis scale. SciQLopPlotAxis::set_log blocks the
    // underlying QCPAxis::scaleTypeChanged, so drive the re-bin (and the UI
    // signal) from the wrapper's log_changed instead.
    if (_keyAxis)
        connect(_keyAxis, &SciQLopPlotAxis::log_changed, this,
                [this](bool on)
                {
                    if (_hist)
                        _hist->refreshBinning();
                    Q_EMIT x_bins_log_changed(on);
                });
    if (_valueAxis)
        connect(_valueAxis, &SciQLopPlotAxis::log_changed, this,
                [this](bool on)
                {
                    if (_hist)
                        _hist->refreshBinning();
                    Q_EMIT y_bins_log_changed(on);
                });

    install_rescale_provider();

    if (auto legend_item = _legend_item(); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                &SciQLopHistogram2D::set_selected);
    }
}

SciQLopHistogram2D::~SciQLopHistogram2D()
{
    if (_hist)
    {
        auto* plot = _plot();
        auto* hist = _hist.data();
        _hist = nullptr;
        if (plot)
            (void)plot->removePlottable(hist);
    }
}

void SciQLopHistogram2D::set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
{
    if (!_hist || !x.is_valid() || !y.is_valid())
        return;

    if (x.flat_size() != y.flat_size())
        throw std::runtime_error(
            "Histogram2D.set_data: x and y must have the same length");

    dispatch_dtype(x.format_code(), [&](auto x_tag) {
        dispatch_dtype(y.format_code(), [&](auto y_tag) {
            using X = typename decltype(x_tag)::type;
            using Y = typename decltype(y_tag)::type;
            const auto* x_ptr = static_cast<const X*>(x.raw_data());
            const auto* y_ptr = static_cast<const Y*>(y.raw_data());
            const int nx = static_cast<int>(x.flat_size());
            const int ny = static_cast<int>(y.flat_size());

            auto source = std::make_shared<
                QCPSoADataSource<std::span<const X>, std::span<const Y>>>(
                std::span<const X>(x_ptr, nx), std::span<const Y>(y_ptr, ny));

            _dataHolder = std::make_shared<DataSourceWithBuffers>(
                DataSourceWithBuffers { x, y, source });
            auto aliased = std::shared_ptr<QCPAbstractDataSource>(_dataHolder, source.get());
            _hist->setDataSource(std::move(aliased));
        });
    });

    if (!_got_first_data && x.flat_size() > 0)
    {
        _got_first_data = true;
        connect(
            &_hist->pipeline(),
            &QCPHistogramPipeline::finished, this,
            [this](uint64_t) {
                _hist->rescaleDataRange();
                Q_EMIT request_rescale();
            },
            Qt::SingleShotConnection);
    }

    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

QList<SciQLopPyBuffer> SciQLopHistogram2D::data() const noexcept
{
    if (_dataHolder)
        return { _dataHolder->x, _dataHolder->y };
    return {};
}

void SciQLopHistogram2D::set_bins(int key_bins, int value_bins)
{
    if (_hist)
    {
        if (_hist->keyBins() == key_bins && _hist->valueBins() == value_bins)
            return;
        _hist->setBins(key_bins, value_bins);
        Q_EMIT bins_changed(key_bins, value_bins);
    }
}

int SciQLopHistogram2D::key_bins() const
{
    return _hist ? _hist->keyBins() : 0;
}

int SciQLopHistogram2D::value_bins() const
{
    return _hist ? _hist->valueBins() : 0;
}

void SciQLopHistogram2D::set_normalization(int normalization)
{
    if (_hist)
    {
        if (static_cast<int>(_hist->normalization()) == normalization)
            return;
        _hist->setNormalization(static_cast<QCPHistogram2D::Normalization>(normalization));
        Q_EMIT normalization_changed(normalization);
    }
}

int SciQLopHistogram2D::normalization() const
{
    return _hist ? static_cast<int>(_hist->normalization()) : 0;
}

// Log binning is just the axis being logarithmic. Driving the axis re-bins (and
// updates the inspector) through the log_changed connection in the constructor.
void SciQLopHistogram2D::set_x_bins_log(bool log)
{
    if (_keyAxis)
        _keyAxis->set_log(log);
}

void SciQLopHistogram2D::set_y_bins_log(bool log)
{
    if (_valueAxis)
        _valueAxis->set_log(log);
}

bool SciQLopHistogram2D::x_bins_log() const
{
    return _keyAxis && _keyAxis->log();
}

bool SciQLopHistogram2D::y_bins_log() const
{
    return _valueAxis && _valueAxis->log();
}

SciQLopPlotRange SciQLopHistogram2D::z_percentile_range(const SciQLopPlotRange& x_range,
                                                        const SciQLopPlotRange& y_range, double low,
                                                        double high) const noexcept
{
    if (!_hist)
        return SciQLopPlotRange();
    // The binned grid (cell counts) is produced asynchronously; if it isn't
    // ready yet, fall back to the plain min/max path (empty range ⇒ nullopt).
    const QCPColorMapData* grid = _hist->pipeline().result();
    if (!grid)
        return SciQLopPlotRange();
    const int nkey = grid->keySize();
    const int nval = grid->valueSize();
    if (nkey <= 0 || nval <= 0)
        return SciQLopPlotRange();

    const double x_lo = std::min(x_range.start(), x_range.stop());
    const double x_hi = std::max(x_range.start(), x_range.stop());
    const double y_lo = std::min(y_range.start(), y_range.stop());
    const double y_hi = std::max(y_range.start(), y_range.stop());

    // Cells are uniform in coordinate space, but under log binning the renderer
    // places them at log positions. Remap each cell centre the same way so the
    // viewport filter matches what is actually drawn.
    const QCPRange kr = grid->keyRange();
    const QCPRange vr = grid->valueRange();
    const bool x_log = x_bins_log() && kr.lower > 0. && kr.upper > kr.lower;
    const bool y_log = y_bins_log() && vr.lower > 0. && vr.upper > vr.lower;
    const auto to_log = [](double linear, const QCPRange& r)
    {
        const double t = (linear - r.lower) / (r.upper - r.lower);
        return std::pow(10., std::log10(r.lower) + t * (std::log10(r.upper) - std::log10(r.lower)));
    };

    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(nkey) * static_cast<std::size_t>(nval));
    for (int i = 0; i < nkey; ++i)
    {
        for (int j = 0; j < nval; ++j)
        {
            double kx = 0., vy = 0.;
            grid->cellToCoord(i, j, &kx, &vy);
            if (x_log)
                kx = to_log(kx, kr);
            if (y_log)
                vy = to_log(vy, vr);
            if (kx < x_lo || kx > x_hi || vy < y_lo || vy > y_hi)
                continue;
            const double zv = grid->cell(i, j);
            if (std::isfinite(zv))
                values.push_back(zv);
        }
    }
    return sciqlop::percentile::percentile_range(values, low, high);
}

SciQLopHistogram2DFunction::SciQLopHistogram2DFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                                       SciQLopPlotAxis* yAxis,
                                                       SciQLopPlotColorScaleAxis* zAxis,
                                                       GetDataPyCallable&& callable,
                                                       const QString& name,
                                                       int key_bins, int value_bins)
    : SciQLopHistogram2D{parent, xAxis, yAxis, zAxis, name, key_bins, value_bins}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

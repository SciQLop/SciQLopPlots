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
#include "SciQLopPlots/constants.hpp"

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

void SciQLopHistogram2D::set_data(PyBuffer x, PyBuffer y)
{
    if (!_hist || !x.is_valid() || !y.is_valid())
        return;

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

QList<PyBuffer> SciQLopHistogram2D::data() const noexcept
{
    if (_dataHolder)
        return { _dataHolder->x, _dataHolder->y };
    return {};
}

void SciQLopHistogram2D::set_bins(int key_bins, int value_bins)
{
    if (_hist)
        _hist->setBins(key_bins, value_bins);
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
        _hist->setNormalization(static_cast<QCPHistogram2D::Normalization>(normalization));
}

int SciQLopHistogram2D::normalization() const
{
    return _hist ? static_cast<int>(_hist->normalization()) : 0;
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

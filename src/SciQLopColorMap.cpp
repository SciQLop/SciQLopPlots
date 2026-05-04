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
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"
#include "SciQLopPlots/Profiling.hpp"
#include "SciQLopPlots/Tracing.hpp"
#include "SciQLopPlots/constants.hpp"

void SciQLopColorMap::_cmap_got_destroyed()
{
    _cmap = nullptr;
}

SciQLopColorMap::SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                 SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                 const QString& name, QVariantMap metaData)
    : SciQLopColorMapBase(xAxis, yAxis, zAxis, std::move(metaData), parent)
{
    _cmap = new QCPColorMap2(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    _cmap->setLayer(Constants::LayersNames::ColorMap);
    connect(_cmap, &QCPColorMap2::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);
    connect(_cmap, &QCPAbstractPlottable::busyChanged,
            this, &SciQLopPlottableInterface::busy_changed);
    SciQLopColorMap::set_gradient(ColorGradient::Jet);
    SciQLopColorMap::set_name(name);

    if (auto legend_item = _legend_item(); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                &SciQLopColorMap::set_selected);
    }
}

SciQLopColorMap::~SciQLopColorMap()
{
    if (_cmap)
    {
        auto* plot = _plot();
        auto* cmap = _cmap.data();
        _cmap = nullptr;
        if (plot)
            (void)plot->removePlottable(cmap);
    }
}

void SciQLopColorMap::set_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    PROFILE_HERE_N("setdata.colormap");
    ::SciQLopPlots::tracing::ScopedZone _sz("setdata.colormap", "setdata");
    _sz.add_arg("nx", static_cast<int64_t>(x.flat_size()));
    _sz.add_arg("ny", static_cast<int64_t>(y.flat_size()));
    _sz.add_arg("nz", static_cast<int64_t>(z.flat_size()));
    if (!_cmap || !x.is_valid() || !y.is_valid() || !z.is_valid())
        return;

    if (x.format_code() != 'd')
        throw std::runtime_error("Keys (x) must be float64");

    const std::size_t nx_sz = x.flat_size();
    const std::size_t ny_sz = y.flat_size();
    const std::size_t nz_sz = z.flat_size();
    // All-empty is allowed and behaves as a clear; otherwise z must be the
    // full nx*ny matrix the QCP datasource expects.
    const bool all_empty = (nx_sz == 0 && ny_sz == 0 && nz_sz == 0);
    if (!all_empty && nz_sz != nx_sz * ny_sz)
        throw std::runtime_error(
            "ColorMap.set_data: z size must equal len(x) * len(y)");

    const auto* x_ptr = static_cast<const double*>(x.raw_data());
    const int nx = static_cast<int>(nx_sz);

    if (nx > 0)
        m_data_range = SciQLopPlotRange(x_ptr[0], x_ptr[nx - 1]);
    else
        m_data_range = SciQLopPlotRange();

    dispatch_dtype(y.format_code(), [&](auto y_tag) {
        dispatch_dtype(z.format_code(), [&](auto z_tag) {
            using Y = typename decltype(y_tag)::type;
            using Z = typename decltype(z_tag)::type;
            const auto* y_ptr = static_cast<const Y*>(y.raw_data());
            const auto* z_ptr = static_cast<const Z*>(z.raw_data());
            const int ny = static_cast<int>(y.flat_size());
            const int nz = static_cast<int>(z.flat_size());

            auto source = std::make_shared<QCPSoADataSource2D<
                std::span<const double>, std::span<const Y>, std::span<const Z>>>(
                std::span<const double>(x_ptr, nx),
                std::span<const Y>(y_ptr, ny),
                std::span<const Z>(z_ptr, nz));

            _dataHolder = std::make_shared<DataSourceWithBuffers>(
                DataSourceWithBuffers{x, y, z, source});
            auto aliased = std::shared_ptr<QCPAbstractDataSource2D>(_dataHolder, source.get());
            _cmap->setDataSource(std::move(aliased));
        });
    });

    check_first_data(nx);

    if (_auto_scale_y && _dataHolder && _dataHolder->source)
    {
        bool found = false;
        auto yRange = _dataHolder->source->yRange(found);
        if (found)
            _valueAxis->qcp_axis()->setRange(yRange);
    }

    Q_EMIT data_changed(x, y, z);
    Q_EMIT data_changed();
}

QList<PyBuffer> SciQLopColorMap::data() const noexcept
{
    if (_dataHolder)
        return {_dataHolder->x, _dataHolder->y, _dataHolder->z};
    return {};
}

void SciQLopColorMap::set_auto_scale_y(bool auto_scale_y)
{
    _auto_scale_y = auto_scale_y;
    Q_EMIT auto_scale_y_changed(auto_scale_y);
}

SciQLopColorMapFunction::SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                                 SciQLopPlotAxis* yAxis,
                                                 SciQLopPlotColorScaleAxis* zAxis,
                                                 GetDataPyCallable&& callable, const QString& name)
    : SciQLopColorMap{parent, xAxis, yAxis, zAxis, name}
    , SciQLopFunctionGraph(std::move(callable), this, 3)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

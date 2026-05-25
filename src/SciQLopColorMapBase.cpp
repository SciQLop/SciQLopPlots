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
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include <algorithm>
#include <cmath>

SciQLopColorMapBase::SciQLopColorMapBase(SciQLopPlotAxis* keyAxis, SciQLopPlotAxis* valueAxis,
                                         SciQLopPlotColorScaleAxis* colorScaleAxis,
                                         QVariantMap metaData, QObject* parent)
    : SciQLopColorMapInterface(std::move(metaData), parent)
    , _keyAxis{keyAxis}
    , _valueAxis{valueAxis}
    , _colorScaleAxis{colorScaleAxis}
{
}

// Derived destructors null their QPointer then remove the plottable,
// preventing double-removal when the base destructor runs.
SciQLopColorMapBase::~SciQLopColorMapBase()
{
    if (_colorScaleAxis)
        _colorScaleAxis->set_rescale_range_provider(nullptr);
}

void SciQLopColorMapBase::set_selected(bool selected) noexcept
{
    if (_selected != selected)
    {
        _selected = selected;
        auto legend_item = _legend_item();
        if (legend_item && legend_item->selected() != selected)
            legend_item->setSelected(selected);
        emit selection_changed(selected);
    }
}

bool SciQLopColorMapBase::selected() const noexcept
{
    return _selected;
}

void SciQLopColorMapBase::set_autoscale_percentile_low(double percentile) noexcept
{
    _autoscale_percentile_low = std::clamp(percentile, 0., 100.);
}

void SciQLopColorMapBase::set_autoscale_percentile_high(double percentile) noexcept
{
    _autoscale_percentile_high = std::clamp(percentile, 0., 100.);
}

std::optional<SciQLopPlotRange> SciQLopColorMapBase::z_rescale_range() const noexcept
{
    if (_autoscale_percentile_low <= 0. && _autoscale_percentile_high >= 100.)
        return std::nullopt;
    if (!_keyAxis || !_valueAxis)
        return std::nullopt;
    auto r = z_percentile_range(_keyAxis->range(), _valueAxis->range(),
                                _autoscale_percentile_low, _autoscale_percentile_high);
    if (std::isnan(r.start()) || std::isnan(r.stop()))
        return std::nullopt;
    return r;
}

void SciQLopColorMapBase::install_rescale_provider() noexcept
{
    if (_colorScaleAxis)
        _colorScaleAxis->set_rescale_range_provider([this]() { return z_rescale_range(); });
}

void SciQLopColorMapBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) {
        if (auto* p = plottable(); p) p->setKeyAxis(a);
    });
}

void SciQLopColorMapBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) {
        if (auto* p = plottable(); p) p->setValueAxis(a);
    });
}

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
#pragma once
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace sciqlop::percentile
{
// [low, high] percentile cutoffs (nearest-rank) of `values`. The vector is
// partially reordered by std::nth_element. Empty input yields a NaN range.
// Shared by color-scale and value-axis robust-autoscale paths.
inline SciQLopPlotRange percentile_range(std::vector<double>& values, double low,
                                         double high) noexcept
{
    if (values.empty())
        return SciQLopPlotRange();

    const auto rank = [n = values.size()](double p) -> std::size_t
    {
        const double clamped = std::clamp(p, 0., 100.);
        const long idx = std::lround(clamped / 100. * static_cast<double>(n - 1));
        return static_cast<std::size_t>(std::clamp<long>(idx, 0, static_cast<long>(n - 1)));
    };
    const std::size_t lo_idx = rank(low);
    const std::size_t hi_idx = rank(high);
    std::nth_element(values.begin(), values.begin() + lo_idx, values.end());
    const double lo_val = values[lo_idx];
    std::nth_element(values.begin(), values.begin() + hi_idx, values.end());
    const double hi_val = values[hi_idx];
    return SciQLopPlotRange(std::min(lo_val, hi_val), std::max(lo_val, hi_val));
}
}

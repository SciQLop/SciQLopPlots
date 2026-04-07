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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace sqp::dsp
{

template <typename T = double>
struct Segment
{
    std::span<const double> x;
    std::span<const T> y; // row-major, n_cols columns
    std::size_t n_cols;
    double median_dt;
};

namespace detail
{
    inline double compute_median_dt(std::span<const double> x)
    {
        if (x.size() < 2)
            return 0.0;

        std::vector<double> dts(x.size() - 1);
        for (std::size_t i = 0; i < dts.size(); ++i)
            dts[i] = x[i + 1] - x[i];

        auto mid = dts.begin() + static_cast<std::ptrdiff_t>(dts.size() / 2);
        std::nth_element(dts.begin(), mid, dts.end());
        return *mid;
    }

    // Find gap boundaries: indices where dt > gap_factor * median_dt
    inline std::vector<std::size_t> find_gap_indices(
        std::span<const double> x, double gap_factor)
    {
        if (x.size() < 2)
            return {};

        const double median_dt = compute_median_dt(x);
        if (median_dt <= 0.0)
            return {};

        const double threshold = gap_factor * median_dt;
        std::vector<std::size_t> gaps;
        for (std::size_t i = 0; i < x.size() - 1; ++i)
        {
            if ((x[i + 1] - x[i]) > threshold)
                gaps.push_back(i + 1);
        }
        return gaps;
    }

} // namespace detail

template <typename T = double>
auto split_segments(
    std::span<const double> x,
    std::span<const T> y,
    std::size_t n_cols,
    double gap_factor = 3.0) -> std::vector<Segment<T>>
{
    if (x.empty())
        return {};

    const auto gaps = detail::find_gap_indices(x, gap_factor);

    std::vector<Segment<T>> segments;
    segments.reserve(gaps.size() + 1);

    std::size_t start = 0;
    for (const auto gap_start : gaps)
    {
        const auto seg_x = x.subspan(start, gap_start - start);
        const auto seg_y = y.subspan(start * n_cols, (gap_start - start) * n_cols);
        segments.push_back(Segment<T> {
            .x = seg_x,
            .y = seg_y,
            .n_cols = n_cols,
            .median_dt = detail::compute_median_dt(seg_x),
        });
        start = gap_start;
    }

    // Last segment
    const auto seg_x = x.subspan(start);
    const auto seg_y = y.subspan(start * n_cols);
    segments.push_back(Segment<T> {
        .x = seg_x,
        .y = seg_y,
        .n_cols = n_cols,
        .median_dt = detail::compute_median_dt(seg_x),
    });

    return segments;
}

} // namespace sqp::dsp

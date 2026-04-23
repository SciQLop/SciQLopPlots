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

#ifndef SQP_DSP_NO_SIMD
#include "SIMD/Primitives.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace sqp::dsp
{

#ifndef SQP_DSP_NO_SIMD
namespace simd
{
    extern decltype(xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(adjacent_diff_t {}))
        dispatched_adjacent_diff;
} // namespace simd
#endif

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
    inline void compute_diffs(const double* x, double* dts, std::size_t n)
    {
#ifndef SQP_DSP_NO_SIMD
        simd::dispatched_adjacent_diff(x, dts, n);
#else
        for (std::size_t i = 0; i < n; ++i)
            dts[i] = x[i + 1] - x[i];
#endif
    }

    inline double compute_median_dt(std::span<const double> x)
    {
        if (x.size() < 2)
            return 0.0;

        const auto n = x.size() - 1;
        std::vector<double> dts(n);
        compute_diffs(x.data(), dts.data(), n);

        // Remove NaN/Inf diffs before nth_element (NaN breaks comparison-based algorithms)
        auto valid_end = std::remove_if(dts.begin(), dts.end(),
            [](double v) { return !std::isfinite(v); });
        if (valid_end == dts.begin())
            return 0.0;
        const auto valid_n = static_cast<std::size_t>(valid_end - dts.begin());
        auto mid = dts.begin() + static_cast<std::ptrdiff_t>(valid_n / 2);
        std::nth_element(dts.begin(), mid, valid_end);
        return *mid;
    }

    inline std::pair<std::vector<std::size_t>, double> find_gaps_and_median(
        std::span<const double> x, double gap_factor)
    {
        if (x.size() < 2)
            return { {}, 0.0 };

        const auto n = x.size() - 1;
        std::vector<double> dts(n);
        compute_diffs(x.data(), dts.data(), n);

        auto valid_end = std::remove_if(dts.begin(), dts.end(),
            [](double v) { return !std::isfinite(v); });
        if (valid_end == dts.begin())
            return { {}, 0.0 };
        const auto valid_n = static_cast<std::size_t>(valid_end - dts.begin());
        auto mid = dts.begin() + static_cast<std::ptrdiff_t>(valid_n / 2);
        std::nth_element(dts.begin(), mid, valid_end);
        const double median_dt = *mid;

        if (median_dt <= 0.0)
            return { {}, median_dt };

        const double threshold = gap_factor * median_dt;
        std::vector<std::size_t> gaps;
        for (std::size_t i = 0; i < n; ++i)
        {
            if ((x[i + 1] - x[i]) > threshold)
                gaps.push_back(i + 1);
        }
        return { std::move(gaps), median_dt };
    }

    // Find gap boundaries: indices where dt > gap_factor * median_dt
    inline std::vector<std::size_t> find_gap_indices(
        std::span<const double> x, double gap_factor)
    {
        return find_gaps_and_median(x, gap_factor).first;
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

    const auto [gaps, global_median_dt] = detail::find_gaps_and_median(x, gap_factor);

    std::vector<Segment<T>> segments;
    segments.reserve(gaps.size() + 1);

    auto make_segment = [&](std::size_t begin, std::size_t end) {
        const auto seg_x = x.subspan(begin, end - begin);
        const auto seg_y = y.subspan(begin * n_cols, (end - begin) * n_cols);
        // Within each segment timestamps are gap-free, so the global
        // median_dt (computed before gap removal) is representative.
        const double seg_median = global_median_dt;
        return Segment<T> {
            .x = seg_x,
            .y = seg_y,
            .n_cols = n_cols,
            .median_dt = seg_median,
        };
    };

    std::size_t start = 0;
    for (const auto gap_start : gaps)
    {
        segments.push_back(make_segment(start, gap_start));
        start = gap_start;
    }
    segments.push_back(make_segment(start, x.size()));

    return segments;
}

} // namespace sqp::dsp

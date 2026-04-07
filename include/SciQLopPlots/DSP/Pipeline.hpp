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

#include "Segments.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

namespace sqp::dsp
{

template <typename T = double>
struct TimeSeries
{
    std::vector<double> x;
    std::vector<T> y; // row-major, n_cols columns
    std::size_t n_cols = 1;

    std::size_t n_rows() const { return x.size(); }

    Segment<T> as_segment() const
    {
        return Segment<T> {
            .x = std::span<const double>(x),
            .y = std::span<const T>(y),
            .n_cols = n_cols,
            .median_dt = detail::compute_median_dt(std::span<const double>(x)),
        };
    }
};

// A stage transforms segments into time series (one per input segment).
// Stages may change length (resampling), but typically preserve n_cols.
// Stages that change dimensionality (FFT) use a different signature.
template <typename T = double>
using Stage = std::function<std::vector<TimeSeries<T>>(const std::vector<Segment<T>>&)>;

namespace detail
{
    // Reassemble segments into a single TimeSeries, inserting NaN at gap boundaries
    template <typename T>
    auto reassemble(const std::vector<TimeSeries<T>>& segments) -> TimeSeries<T>
    {
        if (segments.empty())
            return {};

        if (segments.size() == 1)
            return segments.front();

        // Count total size
        std::size_t total_rows = 0;
        const std::size_t n_cols = segments.front().n_cols;
        for (const auto& seg : segments)
            total_rows += seg.n_rows();

        // Add NaN separator rows between segments
        const std::size_t gap_rows = segments.size() - 1;
        const std::size_t out_rows = total_rows + gap_rows;

        TimeSeries<T> result;
        result.n_cols = n_cols;
        result.x.reserve(out_rows);
        result.y.reserve(out_rows * n_cols);

        for (std::size_t s = 0; s < segments.size(); ++s)
        {
            if (s > 0)
            {
                // Insert NaN separator: midpoint timestamp between segments
                const double gap_t
                    = (segments[s - 1].x.back() + segments[s].x.front()) * 0.5;
                result.x.push_back(gap_t);
                for (std::size_t c = 0; c < n_cols; ++c)
                {
                    if constexpr (std::is_floating_point_v<T>)
                        result.y.push_back(std::numeric_limits<T>::quiet_NaN());
                    else
                        result.y.push_back(T { 0 });
                }
            }
            result.x.insert(result.x.end(), segments[s].x.begin(), segments[s].x.end());
            result.y.insert(result.y.end(), segments[s].y.begin(), segments[s].y.end());
        }

        return result;
    }

} // namespace detail

// Run a pipeline: split data at gaps, apply stages sequentially, reassemble
template <typename T = double>
auto run_pipeline(
    std::span<const double> x,
    std::span<const T> y,
    std::size_t n_cols,
    std::span<const Stage<T>> stages,
    double gap_factor = 3.0) -> TimeSeries<T>
{
    auto segments = split_segments<T>(x, y, n_cols, gap_factor);

    if (segments.empty())
        return {};

    // For the first stage, feed the original segments
    // For subsequent stages, convert previous output back to segments
    std::vector<TimeSeries<T>> current;

    for (const auto& stage : stages)
    {
        if (current.empty())
        {
            // First stage: use original segments
            current = stage(segments);
        }
        else
        {
            // Subsequent stages: convert TimeSeries back to segments
            std::vector<Segment<T>> intermediate;
            intermediate.reserve(current.size());
            for (const auto& ts : current)
                intermediate.push_back(ts.as_segment());
            current = stage(intermediate);
        }
    }

    if (current.empty())
    {
        // No stages — just copy segments as-is
        for (const auto& seg : segments)
        {
            current.push_back(TimeSeries<T> {
                .x = std::vector<double>(seg.x.begin(), seg.x.end()),
                .y = std::vector<T>(seg.y.begin(), seg.y.end()),
                .n_cols = seg.n_cols,
            });
        }
    }

    return detail::reassemble(current);
}

} // namespace sqp::dsp

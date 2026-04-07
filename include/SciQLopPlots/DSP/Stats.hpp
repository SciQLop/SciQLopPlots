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

#include "Parallel.hpp"
#include "Pipeline.hpp"
#include "Segments.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace sqp::dsp
{

// Per-segment block statistics: one value per column per segment.
// Result has 1 row per segment, n_cols columns.
struct BlockStats
{
    double mean = 0.0;
    double variance = 0.0;
    double std_dev = 0.0;
    double min = 0.0;
    double max = 0.0;
    std::size_t count = 0;
};

namespace detail
{
    template <typename T>
    auto block_stats_column(const T* data, std::size_t n_rows, std::size_t n_cols, std::size_t col)
        -> BlockStats
    {
        BlockStats s;
        double mean = 0.0, m2 = 0.0;
        std::size_t count = 0;

        T lo = data[col];
        T hi = data[col];

        for (std::size_t i = 0; i < n_rows; ++i)
        {
            const T val = data[i * n_cols + col];
            if constexpr (std::is_floating_point_v<T>)
            {
                if (std::isnan(val))
                    continue;
            }

            ++count;
            const double d = static_cast<double>(val);
            const double delta = d - mean;
            mean += delta / static_cast<double>(count);
            const double delta2 = d - mean;
            m2 += delta * delta2;

            if (val < lo)
                lo = val;
            if (val > hi)
                hi = val;
        }

        s.mean = mean;
        s.count = count;
        s.min = static_cast<double>(lo);
        s.max = static_cast<double>(hi);
        if (count > 1)
        {
            s.variance = m2 / static_cast<double>(count - 1);
            s.std_dev = std::sqrt(s.variance);
        }
        return s;
    }

    // Rolling mean over a single column using a simple moving average.
    template <typename T>
    void rolling_mean_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        std::size_t window, T* out)
    {
        const auto half = window / 2;

        for (std::size_t i = 0; i < n_rows; ++i)
        {
            const auto lo = (i >= half) ? i - half : std::size_t { 0 };
            const auto hi = std::min(i + half + 1, n_rows);
            double sum = 0.0;
            std::size_t count = 0;
            for (std::size_t j = lo; j < hi; ++j)
            {
                const T val = in[j * n_cols + col];
                if constexpr (std::is_floating_point_v<T>)
                {
                    if (std::isnan(val))
                        continue;
                }
                sum += static_cast<double>(val);
                ++count;
            }
            out[i * n_cols + col] = (count > 0) ? static_cast<T>(sum / static_cast<double>(count))
                                                 : T { 0 };
        }
    }

    // Rolling std dev over a single column using Welford per window.
    template <typename T>
    void rolling_std_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        std::size_t window, T* out)
    {
        const auto half = window / 2;

        for (std::size_t i = 0; i < n_rows; ++i)
        {
            const auto lo = (i >= half) ? i - half : std::size_t { 0 };
            const auto hi = std::min(i + half + 1, n_rows);
            double mean = 0.0, m2 = 0.0;
            std::size_t count = 0;
            for (std::size_t j = lo; j < hi; ++j)
            {
                const T val = in[j * n_cols + col];
                if constexpr (std::is_floating_point_v<T>)
                {
                    if (std::isnan(val))
                        continue;
                }
                ++count;
                const double d = static_cast<double>(val);
                const double delta = d - mean;
                mean += delta / static_cast<double>(count);
                m2 += delta * (d - mean);
            }
            out[i * n_cols + col]
                = (count > 1) ? static_cast<T>(std::sqrt(m2 / static_cast<double>(count - 1)))
                              : T { 0 };
        }
    }

} // namespace detail

// Block statistics per segment per column.
template <typename T = double>
auto block_stats(const std::vector<Segment<T>>& segments)
    -> std::vector<std::vector<BlockStats>>
{
    std::vector<std::vector<BlockStats>> results(segments.size());
    parallel_for(segments.size(), [&](std::size_t i) {
        const auto& seg = segments[i];
        results[i].resize(seg.n_cols);
        for (std::size_t col = 0; col < seg.n_cols; ++col)
            results[i][col]
                = detail::block_stats_column(seg.y.data(), seg.x.size(), seg.n_cols, col);
    });
    return results;
}

// Pipeline stage: rolling mean.
template <typename T = double>
auto rolling_mean(std::size_t window) -> Stage<T>
{
    return [window](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            const auto& seg = segments[i];
            auto& out = results[i];
            out.x.assign(seg.x.begin(), seg.x.end());
            out.y.resize(seg.y.size());
            out.n_cols = seg.n_cols;
            for (std::size_t col = 0; col < seg.n_cols; ++col)
                detail::rolling_mean_column(
                    seg.y.data(), seg.x.size(), seg.n_cols, col, window, out.y.data());
        });
        return results;
    };
}

// Pipeline stage: rolling standard deviation.
template <typename T = double>
auto rolling_std(std::size_t window) -> Stage<T>
{
    return [window](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            const auto& seg = segments[i];
            auto& out = results[i];
            out.x.assign(seg.x.begin(), seg.x.end());
            out.y.resize(seg.y.size());
            out.n_cols = seg.n_cols;
            for (std::size_t col = 0; col < seg.n_cols; ++col)
                detail::rolling_std_column(
                    seg.y.data(), seg.x.size(), seg.n_cols, col, window, out.y.data());
        });
        return results;
    };
}

} // namespace sqp::dsp

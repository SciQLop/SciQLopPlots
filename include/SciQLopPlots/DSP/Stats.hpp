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

    // O(n) rolling mean using a sliding sum.
    // NaN handling: for floating point, NaN values are excluded from the window.
    template <typename T>
    void rolling_mean_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        std::size_t window, T* out)
    {
        if (n_cols > 1)
        {
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            rolling_mean_column(col_in.data(), n_rows, 1, 0, window, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
            return;
        }

        // Single-column (contiguous) path
        const auto half = window / 2;

        double sum = 0.0;
        std::size_t count = 0;
        const auto init_hi = std::min(half + 1, n_rows);
        for (std::size_t j = 0; j < init_hi; ++j)
        {
            const T val = in[j];
            if constexpr (std::is_floating_point_v<T>)
            {
                if (std::isnan(val))
                    continue;
            }
            sum += static_cast<double>(val);
            ++count;
        }
        out[0] = (count > 0) ? static_cast<T>(sum / static_cast<double>(count)) : T { 0 };

        for (std::size_t i = 1; i < n_rows; ++i)
        {
            const auto new_hi = i + half;
            if (new_hi < n_rows)
            {
                const T val = in[new_hi];
                if constexpr (std::is_floating_point_v<T>)
                {
                    if (!std::isnan(val))
                    {
                        sum += static_cast<double>(val);
                        ++count;
                    }
                }
                else
                {
                    sum += static_cast<double>(val);
                    ++count;
                }
            }

            if (i > half)
            {
                const auto old_lo = i - half - 1;
                const T val = in[old_lo];
                if constexpr (std::is_floating_point_v<T>)
                {
                    if (!std::isnan(val))
                    {
                        sum -= static_cast<double>(val);
                        --count;
                    }
                }
                else
                {
                    sum -= static_cast<double>(val);
                    --count;
                }
            }

            out[i] = (count > 0) ? static_cast<T>(sum / static_cast<double>(count)) : T { 0 };
        }
    }

    // O(n) rolling std using sliding sum and sum-of-squares.
    // std = sqrt((sum_sq/n - mean^2) * n/(n-1)) for sample std.
    template <typename T>
    void rolling_std_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        std::size_t window, T* out)
    {
        if (n_cols > 1)
        {
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            rolling_std_column(col_in.data(), n_rows, 1, 0, window, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
            return;
        }

        // Single-column (contiguous) path
        const auto half = window / 2;

        double sum = 0.0, sum_sq = 0.0;
        std::size_t count = 0;
        const auto init_hi = std::min(half + 1, n_rows);
        for (std::size_t j = 0; j < init_hi; ++j)
        {
            const T val = in[j];
            if constexpr (std::is_floating_point_v<T>)
            {
                if (std::isnan(val))
                    continue;
            }
            const double d = static_cast<double>(val);
            sum += d;
            sum_sq += d * d;
            ++count;
        }

        auto emit = [&](std::size_t i)
        {
            if (count > 1)
            {
                const double mean = sum / static_cast<double>(count);
                const double var
                    = (sum_sq - static_cast<double>(count) * mean * mean)
                    / static_cast<double>(count - 1);
                out[i] = static_cast<T>(std::sqrt(std::max(0.0, var)));
            }
            else
            {
                out[i] = T { 0 };
            }
        };

        emit(0);

        for (std::size_t i = 1; i < n_rows; ++i)
        {
            const auto new_hi = i + half;
            if (new_hi < n_rows)
            {
                const T val = in[new_hi];
                if constexpr (std::is_floating_point_v<T>)
                {
                    if (!std::isnan(val))
                    {
                        const double d = static_cast<double>(val);
                        sum += d;
                        sum_sq += d * d;
                        ++count;
                    }
                }
                else
                {
                    const double d = static_cast<double>(val);
                    sum += d;
                    sum_sq += d * d;
                    ++count;
                }
            }

            if (i > half)
            {
                const auto old_lo = i - half - 1;
                const T val = in[old_lo];
                if constexpr (std::is_floating_point_v<T>)
                {
                    if (!std::isnan(val))
                    {
                        const double d = static_cast<double>(val);
                        sum -= d;
                        sum_sq -= d * d;
                        --count;
                    }
                }
                else
                {
                    const double d = static_cast<double>(val);
                    sum -= d;
                    sum_sq -= d * d;
                    --count;
                }
            }

            emit(i);
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

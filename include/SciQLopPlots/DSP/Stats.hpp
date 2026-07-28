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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
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

        // Seeded from the first NON-NaN sample: a NaN seed never loses a
        // comparison, so min/max would stay NaN even with valid data after it
        // (instrument data routinely starts with fill values).
        T lo {};
        T hi {};

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

            if (count == 1)
            {
                lo = val;
                hi = val;
            }
            else
            {
                if (val < lo)
                    lo = val;
                if (val > hi)
                    hi = val;
            }
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

    // Percentile of one column, down the time axis. NaN values are excluded.
    // Uses numpy's default 'linear' interpolation so results match
    // np.nanpercentile exactly. Returns NaN for an all-NaN column
    // (integer types have no NaN, so that case is unreachable for them).
    template <typename T>
    T column_percentile_value(
        const T* data, std::size_t n_rows, std::size_t n_cols, std::size_t col, double q)
    {
        std::vector<T> buf;
        buf.reserve(n_rows);
        for (std::size_t i = 0; i < n_rows; ++i)
        {
            const T val = data[i * n_cols + col];
            if constexpr (std::is_floating_point_v<T>)
            {
                if (std::isnan(val))
                    continue;
            }
            buf.push_back(val);
        }

        if (buf.empty())
        {
            if constexpr (std::is_floating_point_v<T>)
                return std::numeric_limits<T>::quiet_NaN();
            else
                return T { 0 };
        }

        const double idx = q / 100.0 * static_cast<double>(buf.size() - 1);
        const auto lo = static_cast<std::size_t>(idx);
        const double frac = idx - static_cast<double>(lo);

        std::nth_element(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(lo), buf.end());
        const double a = static_cast<double>(buf[lo]);
        // nth_element leaves everything after `lo` >= buf[lo]; the next order
        // statistic is therefore the minimum of that tail.
        const double b = (lo + 1 < buf.size())
            ? static_cast<double>(*std::min_element(
                  buf.begin() + static_cast<std::ptrdiff_t>(lo) + 1, buf.end()))
            : a;
        return static_cast<T>(a + frac * (b - a));
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

    // Rolling percentile via a sorted sliding window (binary-search insert/erase).
    // Window bounds, NaN handling and edge shrinking match rolling_mean_column
    // exactly. DELIBERATE DEVIATION: where rolling_mean_column writes 0 for an
    // all-NaN window, this writes NaN — a 0 background silently turns a
    // difference into a no-op and makes a ratio divide by zero.
    template <typename T>
    void rolling_percentile_column(const T* in, std::size_t n_rows, std::size_t n_cols,
        std::size_t col, std::size_t window, double q, T* out)
    {
        if (n_cols > 1)
        {
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            rolling_percentile_column(col_in.data(), n_rows, 1, 0, window, q, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
            return;
        }

        const auto half = window / 2;
        std::vector<double> win;
        win.reserve(std::min(window + 1, n_rows));

        auto insert = [&](std::size_t j)
        {
            const double v = static_cast<double>(in[j]);
            if (std::isnan(v))
                return;
            win.insert(std::upper_bound(win.begin(), win.end(), v), v);
        };
        auto erase = [&](std::size_t j)
        {
            const double v = static_cast<double>(in[j]);
            if (std::isnan(v))
                return;
            auto it = std::lower_bound(win.begin(), win.end(), v);
            if (it != win.end() && *it == v)
                win.erase(it);
        };
        auto current = [&]() -> T
        {
            if (win.empty())
            {
                if constexpr (std::is_floating_point_v<T>)
                    return std::numeric_limits<T>::quiet_NaN();
                else
                    return T { 0 };
            }
            const double idx = q / 100.0 * static_cast<double>(win.size() - 1);
            const auto lo = static_cast<std::size_t>(idx);
            const double frac = idx - static_cast<double>(lo);
            const double a = win[lo];
            const double b = (lo + 1 < win.size()) ? win[lo + 1] : a;
            return static_cast<T>(a + frac * (b - a));
        };

        const auto init_hi = std::min(half + 1, n_rows);
        for (std::size_t j = 0; j < init_hi; ++j)
            insert(j);
        out[0] = current();

        for (std::size_t i = 1; i < n_rows; ++i)
        {
            const auto new_hi = i + half;
            if (new_hi < n_rows)
                insert(new_hi);
            if (i > half)
                erase(i - half - 1);
            out[i] = current();
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

        auto write_std = [&](std::size_t i)
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

        write_std(0);

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

            write_std(i);
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

// Per-column percentile down the time axis: one value per column.
// Reduces time away, so there is no output time axis.
template <typename T = double>
auto column_percentiles(const T* data, std::size_t n_rows, std::size_t n_cols, double q)
    -> std::vector<T>
{
    std::vector<T> out(n_cols);
    parallel_for(n_cols, [&](std::size_t col)
        { out[col] = detail::column_percentile_value(data, n_rows, n_cols, col, q); });
    return out;
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

// Pipeline stage: rolling percentile (q=50 is the median).
template <typename T = double>
auto rolling_percentile(std::size_t window, double q) -> Stage<T>
{
    return [window, q](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            const auto& seg = segments[i];
            auto& out = results[i];
            out.x.assign(seg.x.begin(), seg.x.end());
            out.y.resize(seg.y.size());
            out.n_cols = seg.n_cols;
            for (std::size_t col = 0; col < seg.n_cols; ++col)
                detail::rolling_percentile_column(
                    seg.y.data(), seg.x.size(), seg.n_cols, col, window, q, out.y.data());
        });
        return results;
    };
}

} // namespace sqp::dsp

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

namespace detail
{
    // Linear interpolation of a single column from irregular to uniform grid.
    // src_x must be sorted. Output written to dst_y.
    template <typename T>
    void interp_column(
        const double* src_x, const T* src_y, std::size_t src_rows, std::size_t src_cols,
        std::size_t col, const double* dst_x, T* dst_y, std::size_t dst_rows, std::size_t dst_cols)
    {
        std::size_t j = 0; // index into source
        for (std::size_t i = 0; i < dst_rows; ++i)
        {
            const double t = dst_x[i];

            // Advance j so that src_x[j] <= t < src_x[j+1]
            while (j + 1 < src_rows && src_x[j + 1] <= t)
                ++j;

            if (j + 1 >= src_rows)
            {
                // Past the end — use last value
                dst_y[i * dst_cols + col] = src_y[(src_rows - 1) * src_cols + col];
            }
            else
            {
                const double x0 = src_x[j];
                const double x1 = src_x[j + 1];
                const double dx = x1 - x0;
                if (dx <= 0.0)
                {
                    dst_y[i * dst_cols + col] = src_y[j * src_cols + col];
                }
                else
                {
                    const double alpha = (t - x0) / dx;
                    const T y0 = src_y[j * src_cols + col];
                    const T y1 = src_y[(j + 1) * src_cols + col];
                    dst_y[i * dst_cols + col]
                        = static_cast<T>(static_cast<double>(y0) * (1.0 - alpha)
                                         + static_cast<double>(y1) * alpha);
                }
            }
        }
    }

    template <typename T>
    auto resample_segment_uniform(const Segment<T>& seg, double target_dt) -> TimeSeries<T>
    {
        if (seg.x.size() < 2)
        {
            return TimeSeries<T> {
                .x = std::vector<double>(seg.x.begin(), seg.x.end()),
                .y = std::vector<T>(seg.y.begin(), seg.y.end()),
                .n_cols = seg.n_cols,
            };
        }

        const double dt = (target_dt > 0.0) ? target_dt : seg.median_dt;
        if (dt <= 0.0)
        {
            return TimeSeries<T> {
                .x = std::vector<double>(seg.x.begin(), seg.x.end()),
                .y = std::vector<T>(seg.y.begin(), seg.y.end()),
                .n_cols = seg.n_cols,
            };
        }

        const double x_start = seg.x.front();
        const double x_end = seg.x.back();
        const auto n_out = static_cast<std::size_t>(std::floor((x_end - x_start) / dt)) + 1;

        TimeSeries<T> out;
        out.n_cols = seg.n_cols;
        out.x.resize(n_out);
        out.y.resize(n_out * seg.n_cols);

        for (std::size_t i = 0; i < n_out; ++i)
            out.x[i] = x_start + static_cast<double>(i) * dt;

        for (std::size_t col = 0; col < seg.n_cols; ++col)
        {
            interp_column(
                seg.x.data(), seg.y.data(), seg.x.size(), seg.n_cols, col,
                out.x.data(), out.y.data(), n_out, out.n_cols);
        }

        return out;
    }

} // namespace detail

// Pipeline stage: resample each segment to a uniform grid.
// target_dt == 0 means use the segment's median_dt.
template <typename T = double>
auto resample_uniform(double target_dt = 0.0) -> Stage<T>
{
    return [target_dt](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            results[i] = detail::resample_segment_uniform(segments[i], target_dt);
        });
        return results;
    };
}

} // namespace sqp::dsp

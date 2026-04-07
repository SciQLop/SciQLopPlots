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
#include <limits>
#include <vector>

namespace sqp::dsp
{

enum class ReduceOp
{
    Sum,
    Mean,
    Min,
    Max,
    Norm, // L2 norm: sqrt(sum(x^2))
};

namespace detail
{
    template <typename T>
    T reduce_row(const T* row, std::size_t n_cols, ReduceOp op)
    {
        switch (op)
        {
            case ReduceOp::Sum:
            {
                T acc = T(0);
                for (std::size_t j = 0; j < n_cols; ++j)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        if (!std::isnan(row[j]))
                            acc += row[j];
                    }
                    else
                    {
                        acc += row[j];
                    }
                }
                return acc;
            }
            case ReduceOp::Mean:
            {
                T acc = T(0);
                std::size_t count = 0;
                for (std::size_t j = 0; j < n_cols; ++j)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        if (std::isnan(row[j]))
                            continue;
                    }
                    acc += row[j];
                    ++count;
                }
                return (count > 0) ? static_cast<T>(static_cast<double>(acc)
                                                     / static_cast<double>(count))
                                   : T(0);
            }
            case ReduceOp::Min:
            {
                T val = std::numeric_limits<T>::max();
                for (std::size_t j = 0; j < n_cols; ++j)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        if (std::isnan(row[j]))
                            continue;
                    }
                    if (row[j] < val)
                        val = row[j];
                }
                return val;
            }
            case ReduceOp::Max:
            {
                T val = std::numeric_limits<T>::lowest();
                for (std::size_t j = 0; j < n_cols; ++j)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        if (std::isnan(row[j]))
                            continue;
                    }
                    if (row[j] > val)
                        val = row[j];
                }
                return val;
            }
            case ReduceOp::Norm:
            {
                double acc = 0.0;
                for (std::size_t j = 0; j < n_cols; ++j)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        if (std::isnan(row[j]))
                            continue;
                    }
                    const double v = static_cast<double>(row[j]);
                    acc += v * v;
                }
                return static_cast<T>(std::sqrt(acc));
            }
        }
        return T(0);
    }

    // Precomputed layout for N-dimensional axis reduction within each row.
    // n_cols = prod(shape). Reduces over 'axes', keeps the rest.
    struct ReduceAxesSpec
    {
        std::size_t n_cols = 0;
        std::size_t out_size = 0;
        std::size_t red_size = 0;
        bool trailing = false; // true when reduced axes are the trailing dimensions

        // Precomputed lookup: input flat index → output flat index.
        // Avoids per-element divisions in the hot loop.
        std::vector<std::size_t> in_to_out;

        ReduceAxesSpec(
            const std::size_t* shape, std::size_t ndim,
            const std::size_t* axes, std::size_t n_axes)
        {
            n_cols = 1;
            for (std::size_t d = 0; d < ndim; ++d)
                n_cols *= shape[d];

            std::vector<bool> is_red(ndim, false);
            for (std::size_t i = 0; i < n_axes; ++i)
                is_red[axes[i]] = true;

            out_size = 1;
            red_size = 1;
            for (std::size_t d = 0; d < ndim; ++d)
            {
                if (is_red[d])
                    red_size *= shape[d];
                else
                    out_size *= shape[d];
            }

            // Check if reduced axes are trailing (contiguous at the end)
            trailing = true;
            {
                std::size_t first_red = ndim;
                for (std::size_t d = 0; d < ndim; ++d)
                {
                    if (is_red[d] && first_red == ndim)
                        first_red = d;
                    if (!is_red[d] && first_red != ndim)
                    {
                        trailing = false;
                        break;
                    }
                }
            }

            if (!trailing)
            {
                // Precompute in→out mapping for non-trailing case.
                // Computed once, reused for every row.
                std::vector<std::size_t> in_strides(ndim);
                in_strides[ndim - 1] = 1;
                for (int d = static_cast<int>(ndim) - 2; d >= 0; --d)
                    in_strides[d] = in_strides[d + 1] * shape[d + 1];

                // Output strides for kept dimensions
                std::vector<std::size_t> kept_out_stride;
                {
                    std::size_t s = 1;
                    for (int d = static_cast<int>(ndim) - 1; d >= 0; --d)
                    {
                        if (!is_red[d])
                        {
                            kept_out_stride.push_back(s);
                            s *= shape[d];
                        }
                    }
                    std::reverse(kept_out_stride.begin(), kept_out_stride.end());
                }

                in_to_out.resize(n_cols);
                for (std::size_t flat = 0; flat < n_cols; ++flat)
                {
                    std::size_t oi = 0;
                    std::size_t tmp = flat;
                    std::size_t ki = 0;
                    for (int d = static_cast<int>(ndim) - 1; d >= 0; --d)
                    {
                        const auto coord = tmp % shape[d];
                        tmp /= shape[d];
                        if (!is_red[d])
                            oi += coord * kept_out_stride[kept_out_stride.size() - 1 - ki++];
                    }
                    in_to_out[flat] = oi;
                }
            }
        }
    };

    // Reduce trailing axes: groups of red_size contiguous elements.
    template <typename T>
    void reduce_axes_trailing(
        const T* row, std::size_t out_size, std::size_t group_size, ReduceOp op, T* out)
    {
        for (std::size_t oi = 0; oi < out_size; ++oi)
            out[oi] = reduce_row(&row[oi * group_size], group_size, op);
    }

    // Reduce non-trailing axes using precomputed lookup table.
    // Single pass over input, scatter-accumulate into output.
    template <typename T>
    void reduce_axes_general(
        const T* row, const ReduceAxesSpec& spec, ReduceOp op, T* out)
    {
        // Initialize output
        switch (op)
        {
            case ReduceOp::Min:
                for (std::size_t i = 0; i < spec.out_size; ++i)
                    out[i] = std::numeric_limits<T>::max();
                break;
            case ReduceOp::Max:
                for (std::size_t i = 0; i < spec.out_size; ++i)
                    out[i] = std::numeric_limits<T>::lowest();
                break;
            default:
                for (std::size_t i = 0; i < spec.out_size; ++i)
                    out[i] = T(0);
                break;
        }

        // Single pass: scatter-accumulate
        for (std::size_t flat = 0; flat < spec.n_cols; ++flat)
        {
            const T val = row[flat];
            if constexpr (std::is_floating_point_v<T>)
            {
                if (std::isnan(val))
                    continue;
            }
            const auto oi = spec.in_to_out[flat];
            switch (op)
            {
                case ReduceOp::Sum:
                case ReduceOp::Mean:
                    out[oi] += val;
                    break;
                case ReduceOp::Min:
                    if (val < out[oi])
                        out[oi] = val;
                    break;
                case ReduceOp::Max:
                    if (val > out[oi])
                        out[oi] = val;
                    break;
                case ReduceOp::Norm:
                    out[oi] += val * val;
                    break;
            }
        }

        // Post-process
        if (op == ReduceOp::Mean)
        {
            const auto divisor = static_cast<double>(spec.red_size);
            for (std::size_t i = 0; i < spec.out_size; ++i)
                out[i] = static_cast<T>(static_cast<double>(out[i]) / divisor);
        }
        else if (op == ReduceOp::Norm)
        {
            for (std::size_t i = 0; i < spec.out_size; ++i)
                out[i] = static_cast<T>(std::sqrt(static_cast<double>(out[i])));
        }
    }

    template <typename T>
    void reduce_axes_row(const T* row, const ReduceAxesSpec& spec, ReduceOp op, T* out)
    {
        if (spec.trailing)
            reduce_axes_trailing(row, spec.out_size, spec.red_size, op, out);
        else
            reduce_axes_general(row, spec, op, out);
    }

} // namespace detail

// Pipeline stage: reduce n_cols → 1 column per row.
template <typename T = double>
auto reduce(ReduceOp op) -> Stage<T>
{
    return [op](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            const auto& seg = segments[i];
            const auto n_rows = seg.x.size();
            auto& out = results[i];
            out.x.assign(seg.x.begin(), seg.x.end());
            out.y.resize(n_rows);
            out.n_cols = 1;

            for (std::size_t row = 0; row < n_rows; ++row)
                out.y[row] = detail::reduce_row(&seg.y[row * seg.n_cols], seg.n_cols, op);
        });
        return results;
    };
}

} // namespace sqp::dsp

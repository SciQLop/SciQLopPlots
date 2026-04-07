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

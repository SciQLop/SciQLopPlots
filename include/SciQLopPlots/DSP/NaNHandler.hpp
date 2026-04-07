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

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace sqp::dsp
{

enum class NaNPolicy
{
    Propagate,   // leave NaN as-is
    Skip,        // ignore NaN in computations (stats/reductions)
    Interpolate, // linear-interpolate isolated NaN from neighbors
};

namespace detail
{

    template <typename T>
    constexpr bool is_nan(T v)
    {
        if constexpr (std::is_floating_point_v<T>)
            return std::isnan(v);
        else
            return false; // int32 has no NaN
    }

    // Interpolate isolated NaN runs (up to max_consecutive) in a single column.
    // Operates in-place on an owning buffer.
    // y_data is column-strided: y_data[row * n_cols + col]
    template <typename T>
    void interpolate_nan_column(
        T* y_data, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        const double* x_data, std::size_t max_consecutive)
    {
        if constexpr (!std::is_floating_point_v<T>)
            return; // nothing to do for integer types

        std::size_t i = 0;
        while (i < n_rows)
        {
            if (!is_nan(y_data[i * n_cols + col]))
            {
                ++i;
                continue;
            }

            // Found a NaN — measure the run length
            const std::size_t run_start = i;
            while (i < n_rows && is_nan(y_data[i * n_cols + col]))
                ++i;
            const std::size_t run_len = i - run_start;

            // Only interpolate short runs with valid neighbors on both sides
            if (run_len > max_consecutive)
                continue;
            if (run_start == 0 || i >= n_rows)
                continue;

            const auto left_idx = run_start - 1;
            const auto right_idx = i;
            const T left_val = y_data[left_idx * n_cols + col];
            const T right_val = y_data[right_idx * n_cols + col];
            const double left_x = x_data[left_idx];
            const double right_x = x_data[right_idx];
            const double dx = right_x - left_x;

            if (dx <= 0.0)
                continue;

            for (std::size_t j = run_start; j < i; ++j)
            {
                const double t = (x_data[j] - left_x) / dx;
                y_data[j * n_cols + col]
                    = static_cast<T>(left_val + t * (right_val - left_val));
            }
        }
    }

} // namespace detail

// Interpolate isolated NaN values in y, using x for linear interpolation.
// Returns a new y vector with NaN runs <= max_consecutive filled in.
// Larger NaN runs are left untouched.
template <typename T>
auto interpolate_nan(
    const double* x_data,
    const T* y_data,
    std::size_t n_rows,
    std::size_t n_cols,
    std::size_t max_consecutive = 1) -> std::vector<T>
{
    std::vector<T> result(y_data, y_data + n_rows * n_cols);
    for (std::size_t col = 0; col < n_cols; ++col)
        detail::interpolate_nan_column(result.data(), n_rows, n_cols, col, x_data, max_consecutive);
    return result;
}

} // namespace sqp::dsp

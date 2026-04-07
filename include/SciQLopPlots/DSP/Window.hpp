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
#include <numbers>
#include <vector>

namespace sqp::dsp
{

enum class WindowType
{
    Rectangular,
    Hann,
    Hamming,
    Blackman,
    FlatTop,
};

// Pre-compute window coefficients for a given size.
template <typename T = double>
auto make_window(std::size_t size, WindowType type) -> std::vector<T>
{
    std::vector<T> w(size);
    if (size == 0)
        return w;
    if (size == 1)
    {
        w[0] = T(1);
        return w;
    }

    const double N = static_cast<double>(size - 1);
    constexpr double two_pi = 2.0 * std::numbers::pi;
    constexpr double four_pi = 4.0 * std::numbers::pi;
    constexpr double six_pi = 6.0 * std::numbers::pi;
    constexpr double eight_pi = 8.0 * std::numbers::pi;

    switch (type)
    {
        case WindowType::Rectangular:
            for (std::size_t i = 0; i < size; ++i)
                w[i] = T(1);
            break;

        case WindowType::Hann:
            for (std::size_t i = 0; i < size; ++i)
                w[i] = static_cast<T>(0.5 * (1.0 - std::cos(two_pi * i / N)));
            break;

        case WindowType::Hamming:
            for (std::size_t i = 0; i < size; ++i)
                w[i] = static_cast<T>(0.54 - 0.46 * std::cos(two_pi * i / N));
            break;

        case WindowType::Blackman:
            for (std::size_t i = 0; i < size; ++i)
                w[i] = static_cast<T>(
                    0.42 - 0.5 * std::cos(two_pi * i / N) + 0.08 * std::cos(four_pi * i / N));
            break;

        case WindowType::FlatTop:
            // Coefficients from: https://en.wikipedia.org/wiki/Window_function#Flat_top_window
            for (std::size_t i = 0; i < size; ++i)
                w[i] = static_cast<T>(
                    0.21557895 - 0.41663158 * std::cos(two_pi * i / N)
                    + 0.277263158 * std::cos(four_pi * i / N)
                    - 0.083578947 * std::cos(six_pi * i / N)
                    + 0.006947368 * std::cos(eight_pi * i / N));
            break;
    }

    return w;
}

// Apply a pre-computed window to data in-place.
// data and window must have the same size.
// Uses SIMD elementwise_mul when available, otherwise scalar.
template <typename T>
void apply_window(T* data, const T* window, std::size_t size)
{
    // Simple scalar — compiler auto-vectorizes this well with -O2.
    // For explicit SIMD, the caller can use simd::dispatched_elementwise_mul directly.
    for (std::size_t i = 0; i < size; ++i)
        data[i] *= window[i];
}

} // namespace sqp::dsp

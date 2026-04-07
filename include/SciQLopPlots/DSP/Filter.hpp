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
#include <span>
#include <vector>

namespace sqp::dsp
{

namespace detail
{

    // Apply FIR filter to a single column of a segment.
    template <typename T>
    void fir_apply_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        const double* coeffs, std::size_t n_taps,
        T* out)
    {
        const auto half = n_taps / 2;

        // Edges: copy input (zero-phase alternative left to the caller)
        for (std::size_t i = 0; i < half && i < n_rows; ++i)
            out[i * n_cols + col] = in[i * n_cols + col];
        for (std::size_t i = n_rows > half ? n_rows - half : 0; i < n_rows; ++i)
            out[i * n_cols + col] = in[i * n_cols + col];

        if (n_cols == 1)
        {
            for (std::size_t i = half; i + half < n_rows; ++i)
            {
                double acc = 0.0;
                for (std::size_t k = 0; k < n_taps; ++k)
                    acc += static_cast<double>(in[i - half + k]) * coeffs[k];
                out[i] = static_cast<T>(acc);
            }
        }
        else
        {
            for (std::size_t i = half; i + half < n_rows; ++i)
            {
                double acc = 0.0;
                for (std::size_t k = 0; k < n_taps; ++k)
                    acc += static_cast<double>(in[(i - half + k) * n_cols + col]) * coeffs[k];
                out[i * n_cols + col] = static_cast<T>(acc);
            }
        }
    }

    // Apply a cascade of biquad sections (SOS) to a single column.
    // Each section: [b0, b1, b2, a0, a1, a2] — standard SOS format.
    // State is reset at segment entry (direct form II transposed).
    template <typename T>
    void sos_apply_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        const double* sos, std::size_t n_sections, T* out)
    {
        // Copy input to output, then filter in-place across sections
        for (std::size_t i = 0; i < n_rows; ++i)
            out[i * n_cols + col] = in[i * n_cols + col];

        for (std::size_t s = 0; s < n_sections; ++s)
        {
            const double b0 = sos[s * 6 + 0];
            const double b1 = sos[s * 6 + 1];
            const double b2 = sos[s * 6 + 2];
            const double a0 = sos[s * 6 + 3];
            const double a1 = sos[s * 6 + 4];
            const double a2 = sos[s * 6 + 5];

            // Normalize by a0
            const double inv_a0 = 1.0 / a0;
            const double nb0 = b0 * inv_a0;
            const double nb1 = b1 * inv_a0;
            const double nb2 = b2 * inv_a0;
            const double na1 = a1 * inv_a0;
            const double na2 = a2 * inv_a0;

            double z1 = 0.0, z2 = 0.0;
            for (std::size_t i = 0; i < n_rows; ++i)
            {
                const double x = static_cast<double>(out[i * n_cols + col]);
                const double y = nb0 * x + z1;
                z1 = nb1 * x - na1 * y + z2;
                z2 = nb2 * x - na2 * y;
                out[i * n_cols + col] = static_cast<T>(y);
            }
        }
    }

} // namespace detail

// Pipeline stage: FIR filter with user-provided coefficients.
// coeffs: filter taps (designed externally, e.g. scipy.signal.firwin).
template <typename T = double>
auto fir_filter(std::span<const double> coeffs) -> Stage<T>
{
    // Copy coefficients so the stage is self-contained
    auto coeffs_vec = std::vector<double>(coeffs.begin(), coeffs.end());

    return [coeffs_vec](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            const auto& seg = segments[i];
            auto& out = results[i];
            out.x.assign(seg.x.begin(), seg.x.end());
            out.y.resize(seg.y.size());
            out.n_cols = seg.n_cols;

            for (std::size_t col = 0; col < seg.n_cols; ++col)
            {
                detail::fir_apply_column(
                    seg.y.data(), seg.x.size(), seg.n_cols, col,
                    coeffs_vec.data(), coeffs_vec.size(), out.y.data());
            }
        });
        return results;
    };
}

// Pipeline stage: IIR filter with user-provided SOS (second-order sections) matrix.
// sos: flat array of [b0,b1,b2,a0,a1,a2] × n_sections (standard scipy SOS format).
// Designed externally, e.g. scipy.signal.butter(..., output='sos').
template <typename T = double>
auto iir_sos(std::span<const double> sos, std::size_t n_sections) -> Stage<T>
{
    auto sos_vec = std::vector<double>(sos.begin(), sos.end());

    return [sos_vec, n_sections](
               const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            const auto& seg = segments[i];
            auto& out = results[i];
            out.x.assign(seg.x.begin(), seg.x.end());
            out.y.resize(seg.y.size());
            out.n_cols = seg.n_cols;

            for (std::size_t col = 0; col < seg.n_cols; ++col)
            {
                detail::sos_apply_column(
                    seg.y.data(), seg.x.size(), seg.n_cols, col,
                    sos_vec.data(), n_sections, out.y.data());
            }
        });
        return results;
    };
}

} // namespace sqp::dsp

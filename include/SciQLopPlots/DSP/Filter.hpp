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
#include "SIMD/Primitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace sqp::dsp
{

#ifndef SQP_DSP_NO_SIMD
namespace simd
{
    // Defined in kernels_dispatch.cpp — runtime-dispatched SIMD dot product.
    extern decltype(xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(dot_product_t {}))
        dispatched_dot_product;
} // namespace simd
#endif

namespace detail
{

    // Apply causal FIR filter to a single column of a segment.
    // Equivalent to scipy.signal.lfilter(coeffs, 1.0, x).
    // For contiguous data (n_cols==1), uses SIMD dot product on the steady-state region.
    template <typename T>
    void fir_apply_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        const T* coeffs, std::size_t n_taps,
        T* out)
    {
        if (n_cols == 1)
        {
            // Ramp-up: i < n_taps, partial overlap with coefficients
            const auto ramp = std::min(n_taps, n_rows);
            for (std::size_t i = 0; i < ramp; ++i)
            {
                T acc = T(0);
                for (std::size_t k = 0; k <= i; ++k)
                    acc += in[i - k] * coeffs[k];
                out[i] = acc;
            }

            // Steady state: full n_taps overlap — use SIMD dot product.
            // in[i-k] for k=0..n_taps-1 is a reversed slice starting at in[i].
            // dot_product(coeffs, &in[i - n_taps + 1], n_taps) gives the same
            // result when coeffs are pre-reversed. Since lfilter convention has
            // coeffs[0] multiply in[i], coeffs[1] multiply in[i-1], etc.,
            // we need dot_product(&in[i - n_taps + 1], reversed_coeffs, n_taps).
            // Instead, just reverse the pointer arithmetic:
            // sum(in[i-k] * coeffs[k]) = sum(in[i-n_taps+1+j] * coeffs[n_taps-1-j])
            // which is dot_product(&in[i-n_taps+1], reversed_coeffs, n_taps).
            //
            // To avoid reversing coefficients, note that for the scalar inner loop
            // the compiler can auto-vectorize well enough. But the SIMD dot_product
            // needs contiguous a[] and b[] arrays. We can prepare reversed coefficients.

#ifndef SQP_DSP_NO_SIMD
            if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
            {
                // Reverse coefficients once for SIMD dot product
                std::vector<T> rcoeffs(n_taps);
                for (std::size_t k = 0; k < n_taps; ++k)
                    rcoeffs[k] = coeffs[n_taps - 1 - k];

                for (std::size_t i = ramp; i < n_rows; ++i)
                    out[i] = static_cast<T>(
                        simd::dispatched_dot_product(&in[i - n_taps + 1], rcoeffs.data(), n_taps));
            }
            else
#endif
            {
                for (std::size_t i = ramp; i < n_rows; ++i)
                {
                    T acc = T(0);
                    for (std::size_t k = 0; k < n_taps; ++k)
                        acc += in[i - k] * coeffs[k];
                    out[i] = acc;
                }
            }
        }
        else
        {
            // Extract column to contiguous buffer, run single-col path, scatter back.
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            fir_apply_column(col_in.data(), n_rows, 1, 0, coeffs, n_taps, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
        }
    }

    // Apply a cascade of biquad sections (SOS) to a single column.
    // Each section: [b0, b1, b2, a0, a1, a2] — standard SOS format.
    // State is reset at segment entry (direct form II transposed).
    template <typename T>
    void sos_apply_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        const T* sos, std::size_t n_sections, T* out)
    {
        if (n_cols > 1)
        {
            // Gather column to contiguous buffer, run single-col path, scatter back.
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            sos_apply_column(col_in.data(), n_rows, 1, 0, sos, n_sections, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
            return;
        }

        // Single-column (contiguous) path
        for (std::size_t i = 0; i < n_rows; ++i)
            out[i] = in[i];

        for (std::size_t s = 0; s < n_sections; ++s)
        {
            const T b0 = sos[s * 6 + 0];
            const T b1 = sos[s * 6 + 1];
            const T b2 = sos[s * 6 + 2];
            const T a0 = sos[s * 6 + 3];
            const T a1 = sos[s * 6 + 4];
            const T a2 = sos[s * 6 + 5];

            if (a0 == T(0))
                continue;
            const T inv_a0 = T(1) / a0;
            const T nb0 = b0 * inv_a0;
            const T nb1 = b1 * inv_a0;
            const T nb2 = b2 * inv_a0;
            const T na1 = a1 * inv_a0;
            const T na2 = a2 * inv_a0;

            T z1 = T(0), z2 = T(0);
            for (std::size_t i = 0; i < n_rows; ++i)
            {
                const T x = out[i];
                const T y = nb0 * x + z1;
                z1 = nb1 * x - na1 * y + z2;
                z2 = nb2 * x - na2 * y;
                out[i] = y;
            }
        }
    }

    // Odd extension: reflect signal at edges and invert around edge value.
    // Matches scipy's filtfilt padtype='odd'.
    template <typename T>
    void odd_extend(const T* in, std::size_t n, std::size_t padlen, std::vector<T>& padded)
    {
        padded.resize(n + 2 * padlen);
        for (std::size_t i = 0; i < padlen; ++i)
            padded[i] = T(2) * in[0] - in[padlen - i];
        for (std::size_t i = 0; i < n; ++i)
            padded[padlen + i] = in[i];
        for (std::size_t i = 0; i < padlen; ++i)
            padded[padlen + n + i] = T(2) * in[n - 1] - in[n - 2 - i];
    }

    // Zero-phase FIR: forward-backward causal filter with odd-extension padding.
    // Equivalent to scipy.signal.filtfilt(coeffs, 1.0, x).
    template <typename T>
    void filtfilt_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        const T* coeffs, std::size_t n_taps, T* out)
    {
        if (n_cols > 1)
        {
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            filtfilt_column(col_in.data(), n_rows, 1, 0, coeffs, n_taps, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
            return;
        }

        const auto padlen = std::min(3 * n_taps, n_rows - 1);
        std::vector<T> padded;
        odd_extend(in, n_rows, padlen, padded);
        const auto plen = padded.size();

        // Forward pass
        std::vector<T> fwd(plen);
        fir_apply_column(padded.data(), plen, 1, 0, coeffs, n_taps, fwd.data());

        // Reverse
        std::reverse(fwd.begin(), fwd.end());

        // Backward pass
        std::vector<T> bwd(plen);
        fir_apply_column(fwd.data(), plen, 1, 0, coeffs, n_taps, bwd.data());

        // Reverse and extract
        std::reverse(bwd.begin(), bwd.end());
        for (std::size_t i = 0; i < n_rows; ++i)
            out[i] = bwd[padlen + i];
    }

    // Compute steady-state initial conditions for SOS cascade (unit step response).
    // Returns {z1, z2} per section, pre-scaled for cascade DC gain.
    // Equivalent to scipy.signal.sosfilt_zi.
    template <typename T>
    auto sosfilt_zi(const T* sos, std::size_t n_sections) -> std::vector<std::pair<T, T>>
    {
        std::vector<std::pair<T, T>> zi(n_sections);
        T scale = T(1);

        for (std::size_t s = 0; s < n_sections; ++s)
        {
            if (sos[s * 6 + 3] == T(0))
                continue;
            const T inv_a0 = T(1) / sos[s * 6 + 3];
            const T b0 = sos[s * 6 + 0] * inv_a0;
            const T b1 = sos[s * 6 + 1] * inv_a0;
            const T b2 = sos[s * 6 + 2] * inv_a0;
            const T a1 = sos[s * 6 + 4] * inv_a0;
            const T a2 = sos[s * 6 + 5] * inv_a0;

            const T denom = T(1) + a1 + a2;
            const T K = (denom != T(0)) ? (b0 + b1 + b2) / denom : T(1);
            zi[s] = {
                ((b1 + b2) - (a1 + a2) * K) * scale,
                (b2 - a2 * K) * scale,
            };
            scale *= K;
        }
        return zi;
    }

    // Apply SOS cascade with initial conditions (contiguous, n_cols==1).
    template <typename T>
    void sos_apply_with_zi(
        const T* in, std::size_t n_rows,
        const T* sos, std::size_t n_sections,
        const std::vector<std::pair<T, T>>& zi, T edge_val,
        T* out)
    {
        for (std::size_t i = 0; i < n_rows; ++i)
            out[i] = in[i];

        for (std::size_t s = 0; s < n_sections; ++s)
        {
            if (sos[s * 6 + 3] == T(0))
                continue;
            const T inv_a0 = T(1) / sos[s * 6 + 3];
            const T nb0 = sos[s * 6 + 0] * inv_a0;
            const T nb1 = sos[s * 6 + 1] * inv_a0;
            const T nb2 = sos[s * 6 + 2] * inv_a0;
            const T na1 = sos[s * 6 + 4] * inv_a0;
            const T na2 = sos[s * 6 + 5] * inv_a0;

            T z1 = zi[s].first * edge_val;
            T z2 = zi[s].second * edge_val;

            for (std::size_t i = 0; i < n_rows; ++i)
            {
                const T x = out[i];
                const T y = nb0 * x + z1;
                z1 = nb1 * x - na1 * y + z2;
                z2 = nb2 * x - na2 * y;
                out[i] = y;
            }
        }
    }

    // Zero-phase IIR (SOS): forward-backward biquad cascade with odd-extension padding
    // and steady-state initial conditions. Equivalent to scipy.signal.sosfiltfilt.
    template <typename T>
    void sosfiltfilt_column(
        const T* in, std::size_t n_rows, std::size_t n_cols, std::size_t col,
        const T* sos, std::size_t n_sections, T* out)
    {
        if (n_cols > 1)
        {
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            sosfiltfilt_column(col_in.data(), n_rows, 1, 0, sos, n_sections, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
            return;
        }

        // padlen = 3*(2*n_sections+1) matches scipy.signal.sosfiltfilt default
        const auto padlen = std::min(3 * (2 * n_sections + 1), n_rows - 1);
        std::vector<T> padded;
        odd_extend(in, n_rows, padlen, padded);
        const auto plen = padded.size();

        auto zi = sosfilt_zi(sos, n_sections);

        // Forward pass with initial conditions scaled by left edge
        std::vector<T> fwd(plen);
        sos_apply_with_zi(padded.data(), plen, sos, n_sections, zi, padded[0], fwd.data());

        // Reverse
        std::reverse(fwd.begin(), fwd.end());

        // Backward pass with initial conditions scaled by right edge (= fwd[0] after reverse)
        std::vector<T> bwd(plen);
        sos_apply_with_zi(fwd.data(), plen, sos, n_sections, zi, fwd[0], bwd.data());

        // Reverse and extract
        std::reverse(bwd.begin(), bwd.end());
        for (std::size_t i = 0; i < n_rows; ++i)
            out[i] = bwd[padlen + i];
    }

} // namespace detail

// Pipeline stage: FIR filter with user-provided coefficients.
// coeffs: filter taps (designed externally, e.g. scipy.signal.firwin).
template <typename T = double>
auto fir_filter(std::span<const T> coeffs) -> Stage<T>
{
    auto coeffs_vec = std::vector<T>(coeffs.begin(), coeffs.end());

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
auto iir_sos(std::span<const T> sos, std::size_t n_sections) -> Stage<T>
{
    auto sos_vec = std::vector<T>(sos.begin(), sos.end());

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

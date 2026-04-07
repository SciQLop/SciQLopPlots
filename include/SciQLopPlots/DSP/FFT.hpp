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
#include "Window.hpp"

#include <pocketfft_hdronly.h>

#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <vector>

namespace sqp::dsp
{

// Result of a per-segment FFT: frequency bins and magnitude per column.
template <typename T = double>
struct FFTResult
{
    std::vector<double> frequencies; // n_freq bins
    std::vector<T> magnitude;       // row-major [n_freq × n_cols]
    std::size_t n_cols = 1;
};

namespace detail
{
    // Compute real-to-complex FFT of a single column, return magnitude.
    template <typename T>
    auto fft_column(const T* data, std::size_t n_rows, std::size_t n_cols, std::size_t col,
                    const T* window) -> std::vector<T>
    {
        // Apply window and copy to contiguous buffer
        std::vector<double> windowed(n_rows);
        for (std::size_t i = 0; i < n_rows; ++i)
            windowed[i] = static_cast<double>(data[i * n_cols + col])
                        * static_cast<double>(window[i]);

        // Real-to-complex FFT
        const pocketfft::shape_t shape { n_rows };
        const pocketfft::stride_t stride_in { sizeof(double) };
        const pocketfft::stride_t stride_out { sizeof(std::complex<double>) };
        const std::size_t n_freq = n_rows / 2 + 1;
        std::vector<std::complex<double>> spectrum(n_freq);

        pocketfft::r2c(shape, stride_in, stride_out, /*axes=*/ { 0 },
                        pocketfft::FORWARD, windowed.data(), spectrum.data(), 1.0);

        // Magnitude (normalized by N)
        const double inv_n = 1.0 / static_cast<double>(n_rows);
        std::vector<T> mag(n_freq);
        for (std::size_t i = 0; i < n_freq; ++i)
            mag[i] = static_cast<T>(std::abs(spectrum[i]) * inv_n);

        return mag;
    }

    template <typename T>
    auto fft_segment(const Segment<T>& seg, WindowType win_type) -> FFTResult<T>
    {
        if (seg.x.size() < 2 || seg.median_dt <= 0.0)
            return {};

        const auto n_rows = seg.x.size();
        const auto n_freq = n_rows / 2 + 1;
        const double fs = 1.0 / seg.median_dt;

        auto window = make_window<T>(n_rows, win_type);

        FFTResult<T> result;
        result.n_cols = seg.n_cols;

        // Frequency axis
        result.frequencies.resize(n_freq);
        for (std::size_t i = 0; i < n_freq; ++i)
            result.frequencies[i] = static_cast<double>(i) * fs / static_cast<double>(n_rows);

        // FFT each column
        result.magnitude.resize(n_freq * seg.n_cols);
        for (std::size_t col = 0; col < seg.n_cols; ++col)
        {
            auto mag = fft_column(seg.y.data(), n_rows, seg.n_cols, col, window.data());
            for (std::size_t i = 0; i < n_freq; ++i)
                result.magnitude[i * seg.n_cols + col] = mag[i];
        }

        return result;
    }

} // namespace detail

// Per-segment FFT. Returns FFTResult per segment (not a pipeline Stage because
// it changes the x-axis semantics from time to frequency).
template <typename T = double>
auto fft(const std::vector<Segment<T>>& segments, WindowType window = WindowType::Hann)
    -> std::vector<FFTResult<T>>
{
    std::vector<FFTResult<T>> results(segments.size());
    parallel_for(segments.size(), [&](std::size_t i) {
        results[i] = detail::fft_segment(segments[i], window);
    });
    return results;
}

// Convenience: FFT as a pipeline Stage that outputs frequency-domain TimeSeries.
// x = frequencies, y = magnitude. Useful for plotting FFT results directly.
template <typename T = double>
auto fft_stage(WindowType window = WindowType::Hann) -> Stage<T>
{
    return [window](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        auto fft_results = fft(segments, window);
        std::vector<TimeSeries<T>> out(fft_results.size());
        for (std::size_t i = 0; i < fft_results.size(); ++i)
        {
            out[i].x = std::move(fft_results[i].frequencies);
            out[i].y = std::move(fft_results[i].magnitude);
            out[i].n_cols = fft_results[i].n_cols;
        }
        return out;
    };
}

} // namespace sqp::dsp

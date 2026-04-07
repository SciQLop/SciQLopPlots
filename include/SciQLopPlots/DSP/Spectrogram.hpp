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
#include <vector>

namespace sqp::dsp
{

template <typename T = double>
struct SpectrogramResult
{
    std::vector<double> t;     // window center times [n_windows]
    std::vector<double> f;     // frequency bins [n_freq]
    std::vector<T> power;      // row-major [n_windows × n_freq]
    std::size_t n_freq = 0;
};

namespace detail
{
    // Compute power spectrum of a single windowed chunk.
    template <typename T>
    auto power_spectrum(const T* data, std::size_t n, const T* window, double inv_n)
        -> std::vector<T>
    {
        std::vector<double> windowed(n);
        for (std::size_t i = 0; i < n; ++i)
            windowed[i] = static_cast<double>(data[i]) * static_cast<double>(window[i]);

        const pocketfft::shape_t shape { n };
        const pocketfft::stride_t stride_in { sizeof(double) };
        const pocketfft::stride_t stride_out { sizeof(std::complex<double>) };
        const std::size_t n_freq = n / 2 + 1;
        std::vector<std::complex<double>> spectrum(n_freq);

        pocketfft::r2c(shape, stride_in, stride_out, { 0 },
                        pocketfft::FORWARD, windowed.data(), spectrum.data(), 1.0);

        std::vector<T> psd(n_freq);
        for (std::size_t i = 0; i < n_freq; ++i)
        {
            const double re = spectrum[i].real() * inv_n;
            const double im = spectrum[i].imag() * inv_n;
            psd[i] = static_cast<T>(re * re + im * im);
        }
        return psd;
    }

    template <typename T>
    auto spectrogram_segment(
        const Segment<T>& seg, std::size_t col,
        std::size_t window_size, std::size_t hop, WindowType win_type) -> SpectrogramResult<T>
    {
        if (seg.x.size() < window_size || seg.median_dt <= 0.0)
            return {};

        const auto n_rows = seg.x.size();
        const double fs = 1.0 / seg.median_dt;
        const std::size_t n_freq = window_size / 2 + 1;
        const double inv_n = 1.0 / static_cast<double>(window_size);

        // Count windows
        std::size_t n_windows = 0;
        for (std::size_t pos = 0; pos + window_size <= n_rows; pos += hop)
            ++n_windows;

        if (n_windows == 0)
            return {};

        auto window = make_window<T>(window_size, win_type);

        SpectrogramResult<T> result;
        result.n_freq = n_freq;
        result.t.resize(n_windows);
        result.f.resize(n_freq);
        result.power.resize(n_windows * n_freq);

        // Frequency axis
        for (std::size_t i = 0; i < n_freq; ++i)
            result.f[i] = static_cast<double>(i) * fs / static_cast<double>(window_size);

        // Extract contiguous column data for single-column case or copy strided
        std::vector<T> col_data;
        const T* col_ptr = nullptr;
        if (seg.n_cols == 1)
        {
            col_ptr = seg.y.data();
        }
        else
        {
            col_data.resize(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_data[i] = seg.y[i * seg.n_cols + col];
            col_ptr = col_data.data();
        }

        // Compute each window — parallel over windows
        // Capture result pointer and col_ptr for the lambda
        auto* power_ptr = result.power.data();
        auto* t_ptr = result.t.data();
        const auto* x_ptr = seg.x.data();
        const auto* win_ptr = window.data();

        parallel_for(n_windows, [=](std::size_t w) {
            const std::size_t pos = w * hop;
            t_ptr[w] = x_ptr[pos + window_size / 2]; // window center time
            auto psd = power_spectrum(col_ptr + pos, window_size, win_ptr, inv_n);
            for (std::size_t f = 0; f < n_freq; ++f)
                power_ptr[w * n_freq + f] = psd[f];
        });

        return result;
    }

} // namespace detail

// Compute spectrogram for each segment, for a given column.
// window_size: FFT window length in samples.
// overlap: number of overlapping samples (0 = window_size/2).
template <typename T = double>
auto spectrogram(
    const std::vector<Segment<T>>& segments,
    std::size_t col = 0,
    std::size_t window_size = 256,
    std::size_t overlap = 0,
    WindowType window = WindowType::Hann) -> std::vector<SpectrogramResult<T>>
{
    const std::size_t hop = (overlap == 0) ? window_size / 2 : window_size - overlap;

    std::vector<SpectrogramResult<T>> results(segments.size());
    // Segments are processed sequentially here because each segment's windows
    // are already parallelized internally.
    for (std::size_t i = 0; i < segments.size(); ++i)
        results[i] = detail::spectrogram_segment(segments[i], col, window_size, hop, window);

    return results;
}

} // namespace sqp::dsp

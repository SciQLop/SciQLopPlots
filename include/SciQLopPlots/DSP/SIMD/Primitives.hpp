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

// SIMD primitives — inner-loop building blocks for DSP algorithms.
// Not a public API. Called by the DSP functions in the parent directory.
//
// Each primitive is a functor compiled per-arch (SSE2/AVX2/AVX-512/NEON64)
// and dispatched at runtime. The DSP algorithm owns the outer loop and calls
// these on the hot inner part, so data stays in registers within the primitive.

#include "Dispatch.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace sqp::dsp::simd
{

// ── Scalar fallbacks ────────────────────────────────────────────────────────
// Used on architectures without SIMD or as tail-loop handlers.

namespace scalar
{

template <typename T>
T dot_product(const T* a, const T* b, std::size_t n)
{
    T acc = T(0);
    for (std::size_t i = 0; i < n; ++i)
        acc += a[i] * b[i];
    return acc;
}

template <typename T>
void elementwise_mul(const T* a, const T* b, T* out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        out[i] = a[i] * b[i];
}

template <typename T>
void elementwise_mul_inplace(T* a, const T* b, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        a[i] *= b[i];
}

template <typename T>
T reduce_sum(const T* data, std::size_t n)
{
    T acc = T(0);
    for (std::size_t i = 0; i < n; ++i)
        acc += data[i];
    return acc;
}

template <typename T>
std::pair<T, T> reduce_min_max(const T* data, std::size_t n)
{
    if (n == 0)
        return { std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN() };
    T lo = data[0], hi = data[0];
    for (std::size_t i = 1; i < n; ++i)
    {
        lo = std::min(lo, data[i]);
        hi = std::max(hi, data[i]);
    }
    return { lo, hi };
}

template <typename T>
T nan_reduce_sum(const T* data, std::size_t n)
{
    T acc = T(0);
    for (std::size_t i = 0; i < n; ++i)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if (!std::isnan(data[i]))
                acc += data[i];
        }
        else
        {
            acc += data[i];
        }
    }
    return acc;
}

template <typename T>
std::size_t nan_count(const T* data, std::size_t n)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if (!std::isnan(data[i]))
                ++count;
        }
        else
        {
            ++count;
        }
    }
    return count;
}

// Welford online: accumulate a block into running (mean, M2, count)
template <typename T>
void welford_accumulate(const T* data, std::size_t n, double& mean, double& m2, std::size_t& count)
{
    for (std::size_t i = 0; i < n; ++i)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if (std::isnan(data[i]))
                continue;
        }
        ++count;
        const double delta = static_cast<double>(data[i]) - mean;
        mean += delta / static_cast<double>(count);
        const double delta2 = static_cast<double>(data[i]) - mean;
        m2 += delta * delta2;
    }
}

} // namespace scalar


// ── SIMD primitives ─────────────────────────────────────────────────────────
// Functor structs compiled per-arch via explicit template instantiation.

#ifndef SQP_DSP_NO_SIMD

// Dot product: sum(a[i] * b[i]) — the FIR convolution inner loop.
struct dot_product_t
{
    template <class Arch>
    double operator()(Arch, const double* a, const double* b, std::size_t n);

    template <class Arch>
    float operator()(Arch, const float* a, const float* b, std::size_t n);
};

template <class Arch>
double dot_product_t::operator()(Arch, const double* a, const double* b, std::size_t n)
{
    using batch_t = xsimd::batch<double, Arch>;
    constexpr auto simd_size = batch_t::size;
    auto acc = batch_t(0.0);
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
        acc = xsimd::fma(batch_t::load_unaligned(a + i), batch_t::load_unaligned(b + i), acc);
    double result = xsimd::reduce_add(acc);
    for (; i < n; ++i)
        result += a[i] * b[i];
    return result;
}

template <class Arch>
float dot_product_t::operator()(Arch, const float* a, const float* b, std::size_t n)
{
    using batch_t = xsimd::batch<float, Arch>;
    constexpr auto simd_size = batch_t::size;
    auto acc = batch_t(0.0f);
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
        acc = xsimd::fma(batch_t::load_unaligned(a + i), batch_t::load_unaligned(b + i), acc);
    float result = xsimd::reduce_add(acc);
    for (; i < n; ++i)
        result += a[i] * b[i];
    return result;
}


// Element-wise multiply: out[i] = a[i] * b[i] — windowing.
struct elementwise_mul_t
{
    template <class Arch>
    void operator()(Arch, const double* a, const double* b, double* out, std::size_t n);

    template <class Arch>
    void operator()(Arch, const float* a, const float* b, float* out, std::size_t n);
};

template <class Arch>
void elementwise_mul_t::operator()(Arch, const double* a, const double* b, double* out, std::size_t n)
{
    using batch_t = xsimd::batch<double, Arch>;
    constexpr auto simd_size = batch_t::size;
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
        (batch_t::load_unaligned(a + i) * batch_t::load_unaligned(b + i)).store_unaligned(out + i);
    for (; i < n; ++i)
        out[i] = a[i] * b[i];
}

template <class Arch>
void elementwise_mul_t::operator()(Arch, const float* a, const float* b, float* out, std::size_t n)
{
    using batch_t = xsimd::batch<float, Arch>;
    constexpr auto simd_size = batch_t::size;
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
        (batch_t::load_unaligned(a + i) * batch_t::load_unaligned(b + i)).store_unaligned(out + i);
    for (; i < n; ++i)
        out[i] = a[i] * b[i];
}


// Reduction: sum — stats, averaging.
struct reduce_sum_t
{
    template <class Arch>
    double operator()(Arch, const double* data, std::size_t n);

    template <class Arch>
    float operator()(Arch, const float* data, std::size_t n);
};

template <class Arch>
double reduce_sum_t::operator()(Arch, const double* data, std::size_t n)
{
    using batch_t = xsimd::batch<double, Arch>;
    constexpr auto simd_size = batch_t::size;
    auto acc = batch_t(0.0);
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
        acc += batch_t::load_unaligned(data + i);
    double result = xsimd::reduce_add(acc);
    for (; i < n; ++i)
        result += data[i];
    return result;
}

template <class Arch>
float reduce_sum_t::operator()(Arch, const float* data, std::size_t n)
{
    using batch_t = xsimd::batch<float, Arch>;
    constexpr auto simd_size = batch_t::size;
    auto acc = batch_t(0.0f);
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
        acc += batch_t::load_unaligned(data + i);
    float result = xsimd::reduce_add(acc);
    for (; i < n; ++i)
        result += data[i];
    return result;
}


// Reduction: min/max pair.
struct reduce_min_max_t
{
    template <class Arch>
    std::pair<double, double> operator()(Arch, const double* data, std::size_t n);

    template <class Arch>
    std::pair<float, float> operator()(Arch, const float* data, std::size_t n);
};

template <class Arch>
std::pair<double, double> reduce_min_max_t::operator()(Arch, const double* data, std::size_t n)
{
    using batch_t = xsimd::batch<double, Arch>;
    constexpr auto simd_size = batch_t::size;
    if (n == 0)
        return { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() };
    auto vmin = batch_t(std::numeric_limits<double>::max());
    auto vmax = batch_t(std::numeric_limits<double>::lowest());
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
    {
        auto v = batch_t::load_unaligned(data + i);
        vmin = xsimd::min(vmin, v);
        vmax = xsimd::max(vmax, v);
    }
    double rmin = xsimd::reduce_min(vmin);
    double rmax = xsimd::reduce_max(vmax);
    for (; i < n; ++i)
    {
        rmin = std::min(rmin, data[i]);
        rmax = std::max(rmax, data[i]);
    }
    return { rmin, rmax };
}

template <class Arch>
std::pair<float, float> reduce_min_max_t::operator()(Arch, const float* data, std::size_t n)
{
    using batch_t = xsimd::batch<float, Arch>;
    constexpr auto simd_size = batch_t::size;
    if (n == 0)
        return { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN() };
    auto vmin = batch_t(std::numeric_limits<float>::max());
    auto vmax = batch_t(std::numeric_limits<float>::lowest());
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
    {
        auto v = batch_t::load_unaligned(data + i);
        vmin = xsimd::min(vmin, v);
        vmax = xsimd::max(vmax, v);
    }
    float rmin = xsimd::reduce_min(vmin);
    float rmax = xsimd::reduce_max(vmax);
    for (; i < n; ++i)
    {
        rmin = std::min(rmin, data[i]);
        rmax = std::max(rmax, data[i]);
    }
    return { rmin, rmax };
}


// NaN-aware sum: branchless via SIMD select.
struct nan_reduce_sum_t
{
    template <class Arch>
    double operator()(Arch, const double* data, std::size_t n);

    template <class Arch>
    float operator()(Arch, const float* data, std::size_t n);
};

template <class Arch>
double nan_reduce_sum_t::operator()(Arch, const double* data, std::size_t n)
{
    using batch_t = xsimd::batch<double, Arch>;
    constexpr auto simd_size = batch_t::size;
    auto acc = batch_t(0.0);
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
    {
        auto v = batch_t::load_unaligned(data + i);
        acc += xsimd::select(xsimd::isnan(v), batch_t(0.0), v);
    }
    double result = xsimd::reduce_add(acc);
    for (; i < n; ++i)
    {
        if (!std::isnan(data[i]))
            result += data[i];
    }
    return result;
}

template <class Arch>
float nan_reduce_sum_t::operator()(Arch, const float* data, std::size_t n)
{
    using batch_t = xsimd::batch<float, Arch>;
    constexpr auto simd_size = batch_t::size;
    auto acc = batch_t(0.0f);
    std::size_t i = 0;
    for (; i + simd_size <= n; i += simd_size)
    {
        auto v = batch_t::load_unaligned(data + i);
        acc += xsimd::select(xsimd::isnan(v), batch_t(0.0f), v);
    }
    float result = xsimd::reduce_add(acc);
    for (; i < n; ++i)
    {
        if (!std::isnan(data[i]))
            result += data[i];
    }
    return result;
}

// ── Extern template declarations per arch ───────────────────────────────────

#define SQP_DSP_EXTERN_PRIMITIVES(ARCH)                                                             \
    extern template double dot_product_t::operator()<ARCH>(ARCH, const double*, const double*,      \
                                                           std::size_t);                             \
    extern template float dot_product_t::operator()<ARCH>(ARCH, const float*, const float*,         \
                                                          std::size_t);                              \
    extern template void elementwise_mul_t::operator()<ARCH>(ARCH, const double*, const double*,    \
                                                             double*, std::size_t);                  \
    extern template void elementwise_mul_t::operator()<ARCH>(ARCH, const float*, const float*,      \
                                                             float*, std::size_t);                   \
    extern template double reduce_sum_t::operator()<ARCH>(ARCH, const double*, std::size_t);        \
    extern template float reduce_sum_t::operator()<ARCH>(ARCH, const float*, std::size_t);          \
    extern template std::pair<double, double> reduce_min_max_t::operator()<ARCH>(                    \
        ARCH, const double*, std::size_t);                                                           \
    extern template std::pair<float, float> reduce_min_max_t::operator()<ARCH>(                      \
        ARCH, const float*, std::size_t);                                                            \
    extern template double nan_reduce_sum_t::operator()<ARCH>(ARCH, const double*, std::size_t);    \
    extern template float nan_reduce_sum_t::operator()<ARCH>(ARCH, const float*, std::size_t);

#ifdef SQP_DSP_ENABLE_SSE2_ARCH
SQP_DSP_EXTERN_PRIMITIVES(xsimd::sse2)
#endif
#ifdef SQP_DSP_ENABLE_AVX2_ARCH
SQP_DSP_EXTERN_PRIMITIVES(xsimd::avx2)
#endif
#ifdef SQP_DSP_ENABLE_AVX512BW_ARCH
SQP_DSP_EXTERN_PRIMITIVES(xsimd::avx512bw)
#endif
#ifdef SQP_DSP_ENABLE_NEON64_ARCH
SQP_DSP_EXTERN_PRIMITIVES(xsimd::neon64)
#endif

#undef SQP_DSP_EXTERN_PRIMITIVES

#endif // SQP_DSP_NO_SIMD

} // namespace sqp::dsp::simd

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
#include <SciQLopPlots/DSP/SIMD/Primitives.hpp>

// Compiled once per arch (SSE2, AVX2, AVX-512, NEON64) with
// -DSQP_DSP_ARCH=<arch> set by the build system.

namespace sqp::dsp::simd
{

// dot_product
template double dot_product_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const double*, const double*, std::size_t);
template float dot_product_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const float*, const float*, std::size_t);

// elementwise_mul
template void elementwise_mul_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const double*, const double*, double*, std::size_t);
template void elementwise_mul_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const float*, const float*, float*, std::size_t);

// reduce_sum
template double reduce_sum_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const double*, std::size_t);
template float reduce_sum_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const float*, std::size_t);

// reduce_min_max
template std::pair<double, double> reduce_min_max_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const double*, std::size_t);
template std::pair<float, float> reduce_min_max_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const float*, std::size_t);

// nan_reduce_sum
template double nan_reduce_sum_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const double*, std::size_t);
template float nan_reduce_sum_t::operator()<xsimd::SQP_DSP_ARCH>(
    xsimd::SQP_DSP_ARCH, const float*, std::size_t);

} // namespace sqp::dsp::simd

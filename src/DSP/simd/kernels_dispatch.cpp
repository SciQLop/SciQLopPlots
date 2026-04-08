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

// Runtime dispatch: resolve to the best available arch at process startup.
// SQP_DSP_XSIMD_ARCH_LIST is defined by the build system.

namespace sqp::dsp::simd
{

decltype(xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(dot_product_t {}))
    dispatched_dot_product = xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(dot_product_t {});
auto dispatched_elementwise_mul = xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(elementwise_mul_t {});
auto dispatched_reduce_sum = xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(reduce_sum_t {});
auto dispatched_reduce_min_max = xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(reduce_min_max_t {});
auto dispatched_nan_reduce_sum = xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(nan_reduce_sum_t {});

decltype(xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(adjacent_diff_t {}))
    dispatched_adjacent_diff = xsimd::dispatch<SQP_DSP_XSIMD_ARCH_LIST>(adjacent_diff_t {});

} // namespace sqp::dsp::simd

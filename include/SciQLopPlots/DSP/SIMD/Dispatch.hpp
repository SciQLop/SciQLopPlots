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

#ifndef SQP_DSP_NO_SIMD
#include <xsimd/xsimd.hpp>
#endif

// SQP_DSP_ARCH is defined per translation unit by the build system (-DSQP_DSP_ARCH=avx2 etc.)
// SQP_DSP_XSIMD_ARCH_LIST is defined in the dispatch TU

namespace sqp::dsp::simd
{

#ifndef SQP_DSP_NO_SIMD

template <class Arch, typename T, typename align_mode>
void store(const auto& batch_data, T* output)
{
    if constexpr (std::is_same_v<align_mode, xsimd::unaligned_mode>)
        batch_data.store_unaligned(output);
    else
        batch_data.store_aligned(output);
}

#endif // SQP_DSP_NO_SIMD

} // namespace sqp::dsp::simd

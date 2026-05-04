/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Tracing.hpp"

// NeoQCP's Profiling.hpp may have defined the PROFILE_* macros first (it ships
// in a subproject and is pulled in transitively via QCustomPlot headers).
// NeoQCP guards its own #defines with #ifndef (since SciQLop/NeoQCP#26), so it
// won't override us if we're included first — but if NeoQCP loads first in a
// TU, its definitions are in place and our redefines would warn. Undefine
// them so the SciQLopPlots versions — which also feed the runtime Chrome JSON
// tracer — always win regardless of include order.
#ifdef PROFILE_HERE
#  undef PROFILE_HERE
#endif
#ifdef PROFILE_HERE_N
#  undef PROFILE_HERE_N
#endif
#ifdef PROFILE_HERE_NC
#  undef PROFILE_HERE_NC
#endif
#ifdef PROFILE_PASS_VALUE
#  undef PROFILE_PASS_VALUE
#endif

#ifdef TRACY_ENABLE
#  include <tracy/Tracy.hpp>
#  define SCIQLOP_TRACY_ZONE() ZoneScoped
#  define SCIQLOP_TRACY_ZONE_N(name) ZoneScopedN(name)
#  define SCIQLOP_TRACY_ZONE_NC(name, color) ZoneScopedNC(name, color)
#  define PROFILE_PASS_VALUE(value) ZoneValue(value)
#  ifdef PROFILE_MEMORY
#    include <cstddef>
#    include <new>
void* operator new(std::size_t n);
void operator delete(void* ptr) noexcept;
#  endif
#else
#  define SCIQLOP_TRACY_ZONE()              ((void)0)
#  define SCIQLOP_TRACY_ZONE_N(name)        ((void)0)
#  define SCIQLOP_TRACY_ZONE_NC(name, color) ((void)0)
#  define PROFILE_PASS_VALUE(value)         ((void)0)
#endif

#define PROFILE_HERE              SCIQLOP_TRACE_ZONE(__func__);              SCIQLOP_TRACY_ZONE()
#define PROFILE_HERE_N(name)      SCIQLOP_TRACE_ZONE(name);                  SCIQLOP_TRACY_ZONE_N(name)
#define PROFILE_HERE_NC(name, color) SCIQLOP_TRACE_ZONE(name);               SCIQLOP_TRACY_ZONE_NC(name, color)

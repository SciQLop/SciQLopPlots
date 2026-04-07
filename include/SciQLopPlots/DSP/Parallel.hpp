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

#include <BS_thread_pool.hpp>

#include <cstddef>

namespace sqp::dsp
{

// Process-wide thread pool for DSP work. Lazy-initialized on first use.
// Thread count matches hardware_concurrency.
inline BS::light_thread_pool& pool()
{
    static BS::light_thread_pool instance;
    return instance;
}

// Apply f(index) for index in [0, count), distributed across the thread pool.
// Blocks until all tasks complete. For count <= 1, runs inline.
template <typename F>
void parallel_for(std::size_t count, F&& f)
{
    if (count == 0)
        return;
    if (count == 1)
    {
        f(std::size_t { 0 });
        return;
    }
    pool().detach_sequence(std::size_t { 0 }, count, std::forward<F>(f));
    pool().wait();
}

// Convenience: parallel_for over a container with element access.
template <typename Container, typename F>
void parallel_for_each(Container& items, F&& f)
{
    parallel_for(items.size(), [&](std::size_t i) { f(items[i]); });
}

} // namespace sqp::dsp

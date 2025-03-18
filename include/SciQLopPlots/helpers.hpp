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

/*Apply a function to a QPointer if it is not null and return the result or a default value
*/
inline auto qptr_apply_or(auto&& ptr, auto&& func) -> decltype(func(ptr))
{
    if (!ptr.isNull()) [[likely]]
        return func(ptr);
    else [[unlikely]]
        return decltype(func(ptr)) {};
}

inline auto qptr_apply_or(auto&& ptr, auto&& func, auto&& default_value) -> decltype(func(ptr))
{
    if (!ptr.isNull()) [[likely]]
        return func(ptr);
    else [[unlikely]]
        return default_value;
}

/* Apply a function to a QPointer if it is not null */
inline auto qptr_apply(auto&& ptr, auto&& func) -> void
{
    if (!ptr.isNull()) [[likely]]
        func(ptr);
}


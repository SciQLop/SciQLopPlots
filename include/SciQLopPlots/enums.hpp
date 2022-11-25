/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2021, Plasma Physics Laboratory - CNRS
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
#include "cpp_utils/types/detectors.hpp"


namespace SciQLopPlots::enums
{
enum class Axis
{
    x = 0,
    y = 1,
    z = 2
};

enum class DataOrder
{
    x_first = 0,
    y_first = 1
};

enum class Layers
{
    EditMode = 0,
    Cursors = 1,
    Shapes = 2,
    Background = 3
};

enum class IteractionsMode
{
    Normal = 0,
    ObjectCreation = 1
};

enum class GraphType
{
    Line = 0,
    MultiLine = 1,
    ColorMap = 2
};

}


namespace SciQLopPlots::tags
{
struct absolute
{
};
struct relative
{
};

IS_T(is_absolute, absolute);
IS_T(is_relative, relative);
}

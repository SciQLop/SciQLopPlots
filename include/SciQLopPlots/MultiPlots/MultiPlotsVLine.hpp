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

#include "../Items/SciQLopStraightLines.hpp"
#include "SciQLopMultiPlotPanel.hpp"

/*! The old name of a panel-wide vertical line: a SciQLopStraightLine at panel scope whose parts
    are SciQLopVerticalLines. Grey and draggable by default, as before; its Python properties are
    in bindings.xml. Built by its deprecated constructor; Plan B removes it. */
class MultiPlotsVerticalLine : public SciQLopStraightLine
{
    Q_OBJECT

public:
    MultiPlotsVerticalLine(SciQLopMultiPlotPanel* panel, double position,
                           QColor color = QColor::fromRgb(100, 100, 100), bool read_only = false,
                           bool visible = true, const QString tool_tip = "")
            : SciQLopStraightLine(panel, position, Qt::Orientation::Vertical, color, read_only,
                                  visible, tool_tip, Coordinates::Data, true)
    {
    }
};

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

#include "SciQLopPlots/Items/SciQLopSpan.hpp"

/*! The old name of a plot's rectangular span: a SciQLopSpan bounding both axes. Built by its
    deprecated constructor; the factories return plain SciQLopSpans. Plan B removes it. */
class SciQLopRectangularSpan : public SciQLopSpan
{
    Q_OBJECT

public:
    SciQLopRectangularSpan(SciQLopPlot* plot, SciQLopPlotRange key_range,
                           SciQLopPlotRange value_range,
                           QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false,
                           bool visible = true, const QString& tooltip = "")
            : SciQLopSpan(plot, SpanKind::Rectangular, key_range, value_range, color, read_only,
                          visible, tooltip, Coordinates::Data)
    {
    }
};

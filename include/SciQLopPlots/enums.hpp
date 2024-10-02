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
#include <QObject>
#include <QString>

enum class AxisType
{
    NoneAxis,
    TimeAxis,
    XAxis,
    YAxis,
    ZAxis
};

enum class DataOrder
{
    RowMajor,
    ColumnMajor
};

enum class GraphType
{
    Line,
    ParametricCurve,
    ColorMap
};

enum class PlotType
{
    BasicXY,
    TimeSeries
};

enum class GraphMarkerShape
{
    NoMarker,
    Circle,
    Square,
    Triangle,
    Diamond,
    Star,
    Plus,
    Cross,
    X,
    Custom
};


enum class GraphLineStyle
{
    NoLine,
    Line,
    StepLeft,
    StepRight,
    StepCenter
};

Q_DECLARE_METATYPE(GraphLineStyle);

inline GraphLineStyle graph_line_style_from_string(const QString& str)
{
    if (str == "Line")
        return GraphLineStyle::Line;
    if (str == "StepLeft")
        return GraphLineStyle::StepLeft;
    if (str == "StepRight")
        return GraphLineStyle::StepRight;
    if (str == "StepCenter")
        return GraphLineStyle::StepCenter;
    return GraphLineStyle::NoLine;
}

inline QString graph_line_style_to_string(GraphLineStyle style)
{
    switch (style)
    {
        case GraphLineStyle::Line:
            return "Line";
        case GraphLineStyle::StepLeft:
            return "StepLeft";
        case GraphLineStyle::StepRight:
            return "StepRight";
        case GraphLineStyle::StepCenter:
            return "StepCenter";
        default:
            return "NoLine";
    }
}

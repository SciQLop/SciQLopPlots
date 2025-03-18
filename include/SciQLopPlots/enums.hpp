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
#include <magic_enum/magic_enum.hpp>

template <typename T>
T from_string(const QString& str);

#define ENUM_TO_STRING(enum_type)                                                                  \
    inline QString to_string(enum_type value)                                                      \
    {                                                                                              \
        return QString::fromLatin1(magic_enum::enum_name(value));                                  \
    }

#define STRING_TO_ENUM(enum_type)                                                                  \
    template <>                                                                                    \
    inline enum_type from_string<enum_type>(const QString& str)                                    \
    {                                                                                              \
        return magic_enum::enum_cast<enum_type>(str.toStdString())                                 \
            .value_or(static_cast<enum_type>(0));                                                  \
    }

#define ADD_ENUM(enum_type)                                                                        \
    ENUM_TO_STRING(enum_type)                                                                      \
    STRING_TO_ENUM(enum_type)


enum class Coordinates
{
    Pixels,
    Data
};
ADD_ENUM(Coordinates)
Q_DECLARE_METATYPE(Coordinates);

enum class ParameterType
{
    NotAParameter,
    Scalar,
    Vector,
    Multicomponents,
    Spectrogram
};
ADD_ENUM(ParameterType)
Q_DECLARE_METATYPE(ParameterType);

enum class AxisType
{
    NoneAxis,
    TimeAxis,
    XAxis,
    YAxis,
    ZAxis
};
ADD_ENUM(AxisType)
Q_DECLARE_METATYPE(AxisType);

enum class DataOrder
{
    RowMajor,
    ColumnMajor
};
ADD_ENUM(DataOrder)
Q_DECLARE_METATYPE(DataOrder);

enum class GraphType
{
    Line,
    ParametricCurve,
    Scatter,
    ColorMap
};
ADD_ENUM(GraphType)
Q_DECLARE_METATYPE(GraphType);

enum class PlotType
{
    BasicXY,
    TimeSeries,
    Projections,
};
ADD_ENUM(PlotType)
Q_DECLARE_METATYPE(PlotType);

enum class GraphMarkerShape
{
    NoMarker,
    Dot, // .
    Circle, // ○
    Square, // □
    Triangle, // △
    Diamond, // ◇
    Star, // *
    Plus, // +
    Cross, // X
    FilledCircle, // ●
    InvertedTriangle, // ▽
    CrossedSquare, // ☒
    PlusSquare, // ⊞
    CrossedCircle, // ☒
    PlusCircle, // ⊕
    Custom
};
ADD_ENUM(GraphMarkerShape)
Q_DECLARE_METATYPE(GraphMarkerShape);

inline QString GraphMarkerShapeToUTF8(GraphMarkerShape shape)
{
    switch (shape)
    {
    case GraphMarkerShape::NoMarker:
        return "";
    case GraphMarkerShape::Circle:
        return "○";
    case GraphMarkerShape::Dot:
        return ".";
    case GraphMarkerShape::Square:
        return "□";
    case GraphMarkerShape::Triangle:
        return "△";
    case GraphMarkerShape::Diamond:
        return "◇";
    case GraphMarkerShape::Star:
        return "*";
    case GraphMarkerShape::Plus:
        return "+";
    case GraphMarkerShape::Cross:
        return "X";
    case GraphMarkerShape::FilledCircle:
        return "●";
    case GraphMarkerShape::InvertedTriangle:
        return "▽";
    case GraphMarkerShape::CrossedSquare:
        return "☒";
    case GraphMarkerShape::PlusSquare:
        return "⊞";
    case GraphMarkerShape::CrossedCircle:
        return "☒";
    case GraphMarkerShape::PlusCircle:
        return "⊕";
    case GraphMarkerShape::Custom:
        return "";
    }
}

enum class GraphLineStyle
{
    NoLine,
    Line,
    StepLeft,
    StepRight,
    StepCenter
};
ADD_ENUM(GraphLineStyle)
Q_DECLARE_METATYPE(GraphLineStyle);

enum class ColorGradient
{
    Grayscale,
    Hot,
    Cold,
    Night,
    Candy,
    Geography,
    Ion,
    Thermal,
    Polar,
    Spectrum,
    Jet,
    Hues
};
ADD_ENUM(ColorGradient)
Q_DECLARE_METATYPE(ColorGradient);

enum class LineTermination
{
    NoneTermination,
    Arrow,
    SPikeArrow,
    LineArrow,
    Circle,
    Square,
    Diamond,
    Bar,
    HalfBar,
    SkewedBar
};
ADD_ENUM(LineTermination)
Q_DECLARE_METATYPE(LineTermination);

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
#include <array>
#include <magic_enum/magic_enum.hpp>
#include <type_traits>

template <typename T>
    requires std::is_enum_v<T>
inline QString to_string(T value)
{
    return QString::fromLatin1(magic_enum::enum_name(value));
}

template <typename T>
    requires std::is_enum_v<T>
inline T from_string(const QString& str)
{
    return magic_enum::enum_cast<T>(str.toStdString()).value_or(static_cast<T>(0));
}


enum class Coordinates
{
    Pixels,
    Data
};
Q_DECLARE_METATYPE(Coordinates);

enum class ParameterType
{
    NotAParameter,
    Scalar,
    Vector,
    Multicomponents,
    Spectrogram
};
Q_DECLARE_METATYPE(ParameterType);

enum class AxisType
{
    NoneAxis,
    TimeAxis,
    XAxis,
    YAxis,
    ZAxis
};
Q_DECLARE_METATYPE(AxisType);

enum class DataOrder
{
    RowMajor,
    ColumnMajor
};
Q_DECLARE_METATYPE(DataOrder);

enum class GraphType
{
    Line,
    ParametricCurve,
    Scatter,
    ColorMap
};
Q_DECLARE_METATYPE(GraphType);

enum class PlotType
{
    BasicXY,
    TimeSeries,
    Projections,
};
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
Q_DECLARE_METATYPE(GraphMarkerShape);

inline constexpr std::array marker_shape_glyphs = {
    "",  // NoMarker
    ".", // Dot
    "○", // Circle
    "□", // Square
    "△", // Triangle
    "◇", // Diamond
    "*", // Star
    "+", // Plus
    "X", // Cross
    "●", // FilledCircle
    "▽", // InvertedTriangle
    "☒", // CrossedSquare
    "⊞", // PlusSquare
    "☒", // CrossedCircle
    "⊕", // PlusCircle
    "",  // Custom
};
static_assert(marker_shape_glyphs.size() == magic_enum::enum_count<GraphMarkerShape>());

inline QString GraphMarkerShapeToUTF8(GraphMarkerShape shape)
{
    return QString::fromUtf8(marker_shape_glyphs[static_cast<int>(shape)]);
}

enum class GraphLineStyle
{
    NoLine,
    Line,
    StepLeft,
    StepRight,
    StepCenter
};
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
Q_DECLARE_METATYPE(LineTermination);

enum class OverlayLevel
{
    Info,
    Warning,
    Error
};
Q_DECLARE_METATYPE(OverlayLevel);

enum class OverlaySizeMode
{
    Compact,
    FitContent,
    FullWidget
};
Q_DECLARE_METATYPE(OverlaySizeMode);

enum class OverlayPosition
{
    Top,
    Bottom,
    Left,
    Right
};
Q_DECLARE_METATYPE(OverlayPosition);

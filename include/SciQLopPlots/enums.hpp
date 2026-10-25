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

//! The axes a span bounds, fixed when it is made: the key axis (x), the value axis (y), or both.
enum class SpanKind
{
    Vertical,
    Horizontal,
    Rectangular
};
Q_DECLARE_METATYPE(SpanKind);

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

enum class GraphType
{
    Line,
    ParametricCurve,
    Scatter,
    ColorMap,
    Waterfall,
    Histogram2D,
    Histogram
};
Q_DECLARE_METATYPE(GraphType);

//! The factory that makes this kind, as Python users call it; unsupported-call messages name it.
inline constexpr const char* factory_name(GraphType graph_type)
{
    constexpr std::array names { "line",      "parametric_curve", "scatter",  "colormap",
                                 "waterfall", "histogram2d",      "histogram" };
    static_assert(names.size() == static_cast<std::size_t>(GraphType::Histogram) + 1);
    return names[static_cast<std::size_t>(graph_type)];
}

enum class WaterfallOffsetMode
{
    Uniform,
    Custom
};
Q_DECLARE_METATYPE(WaterfallOffsetMode);

enum class PlotType
{
    BasicXY,
    TimeSeries,
    Projections,
};
Q_DECLARE_METATYPE(PlotType);

// How a projection plot's axes follow each other.
enum class ProjectionAxisLinking
{
    Independent,
    ByDimension, // axes showing the same dimension share their range
    SameScale,   // and every axis has the same span: one data unit, one length everywhere
};
Q_DECLARE_METATYPE(ProjectionAxisLinking);

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

enum class HistogramNormalization
{
    Count,
    Density,
    Probability
};
Q_DECLARE_METATYPE(HistogramNormalization);

enum class HistogramStyle
{
    Bars,
    Step
};
Q_DECLARE_METATYPE(HistogramStyle);

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
    Hues,
    // Appended: the values above are stable.
    Viridis,
    Cividis,
    Magma,
    Inferno,
    Plasma,
    Turbo,
    Coolwarm
};
Q_DECLARE_METATYPE(ColorGradient);

enum class LineTermination
{
    NoneTermination,
    Arrow,
    SpikeArrow,
    LineArrow,
    Circle,
    Square,
    Diamond,
    Bar,
    HalfBar,
    SkewedBar,
    SPikeArrow = SpikeArrow // the old misspelling stays for existing callers
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

enum class TimelineStyle
{
    Wave,
    Bars
};
Q_DECLARE_METATYPE(TimelineStyle);

//! How overlapping intervals of one lane are laid out. Off, not None: None is a Python keyword.
enum class TimelineStack
{
    Off,
    Time,
    Category
};
Q_DECLARE_METATYPE(TimelineStack);

enum class TimelineEditMode
{
    Move,
    Resize,
    ChangeLane,
    Create,
    Delete
};
Q_DECLARE_METATYPE(TimelineEditMode);

//! Not an API: sets the kind shiboken gives the global enums above, see bindings.xml.
namespace SciQLopEnumKindAnchor
{
enum class Kind
{
    Int
};
}

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

enum class Coordinates {
    Pixels,
    Data
};

enum class ParameterType
{
    NotAParameter,
    Scalar,
    Vector,
    Multicomponents,
    Spectrogram
};

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
    Scatter,
    ColorMap
};

enum class PlotType
{
    BasicXY,
    TimeSeries,
    Projections,
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

inline ColorGradient color_gradient_from_string(const QString& str)
{
    if (str == "Grayscale")
        return ColorGradient::Grayscale;
    if (str == "Hot")
        return ColorGradient::Hot;
    if (str == "Cold")
        return ColorGradient::Cold;
    if (str == "Night")
        return ColorGradient::Night;
    if (str == "Candy")
        return ColorGradient::Candy;
    if (str == "Geography")
        return ColorGradient::Geography;
    if (str == "Ion")
        return ColorGradient::Ion;
    if (str == "Thermal")
        return ColorGradient::Thermal;
    if (str == "Polar")
        return ColorGradient::Polar;
    if (str == "Spectrum")
        return ColorGradient::Spectrum;
    if (str == "Jet")
        return ColorGradient::Jet;
    if (str == "Hues")
        return ColorGradient::Hues;
    return ColorGradient::Grayscale;
}

inline QString color_gradient_to_string(ColorGradient gradient)
{
    switch (gradient)
    {
        case ColorGradient::Grayscale:
            return "Grayscale";
        case ColorGradient::Hot:
            return "Hot";
        case ColorGradient::Cold:
            return "Cold";
        case ColorGradient::Night:
            return "Night";
        case ColorGradient::Candy:
            return "Candy";
        case ColorGradient::Geography:
            return "Geography";
        case ColorGradient::Ion:
            return "Ion";
        case ColorGradient::Thermal:
            return "Thermal";
        case ColorGradient::Polar:
            return "Polar";
        case ColorGradient::Spectrum:
            return "Spectrum";
        case ColorGradient::Jet:
            return "Jet";
        case ColorGradient::Hues:
            return "Hues";
        default:
            return "Grayscale";
    }
}

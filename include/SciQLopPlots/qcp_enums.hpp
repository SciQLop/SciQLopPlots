/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/enums.hpp"
#include "qcustomplot.h"
#include <magic_enum/magic_enum.hpp>

inline QCPScatterStyle::ScatterShape to_qcp(GraphMarkerShape marker)
{
    switch (marker)
    {
        case GraphMarkerShape::NoMarker:
            return QCPScatterStyle::ScatterShape::ssNone;
        case GraphMarkerShape::Dot:
            return QCPScatterStyle::ScatterShape::ssDot;
        case GraphMarkerShape::Circle:
            return QCPScatterStyle::ScatterShape::ssCircle;
        case GraphMarkerShape::Square:
            return QCPScatterStyle::ScatterShape::ssSquare;
        case GraphMarkerShape::Triangle:
            return QCPScatterStyle::ScatterShape::ssTriangle;
        case GraphMarkerShape::Diamond:
            return QCPScatterStyle::ScatterShape::ssDiamond;
        case GraphMarkerShape::Star:
            return QCPScatterStyle::ScatterShape::ssStar;
        case GraphMarkerShape::Plus:
            return QCPScatterStyle::ScatterShape::ssPlus;
        case GraphMarkerShape::Cross:
            return QCPScatterStyle::ScatterShape::ssCross;
        case GraphMarkerShape::FilledCircle:
            return QCPScatterStyle::ScatterShape::ssDisc;
        case GraphMarkerShape::InvertedTriangle:
            return QCPScatterStyle::ScatterShape::ssTriangleInverted;
        case GraphMarkerShape::CrossedSquare:
            return QCPScatterStyle::ScatterShape::ssCrossSquare;
        case GraphMarkerShape::PlusSquare:
            return QCPScatterStyle::ScatterShape::ssPlusSquare;
        case GraphMarkerShape::CrossedCircle:
            return QCPScatterStyle::ScatterShape::ssCrossCircle;
        case GraphMarkerShape::PlusCircle:
            return QCPScatterStyle::ScatterShape::ssPlusCircle;
        case GraphMarkerShape::Custom:
            return QCPScatterStyle::ScatterShape::ssCustom;
        default:
            return QCPScatterStyle::ScatterShape::ssNone;
    }
}

inline GraphMarkerShape from_qcp(QCPScatterStyle::ScatterShape shape)
{
    switch (shape)
    {
        case QCPScatterStyle::ScatterShape::ssNone:
            return GraphMarkerShape::NoMarker;
        case QCPScatterStyle::ScatterShape::ssCircle:
            return GraphMarkerShape::Circle;
        case QCPScatterStyle::ScatterShape::ssSquare:
            return GraphMarkerShape::Square;
        case QCPScatterStyle::ScatterShape::ssTriangle:
            return GraphMarkerShape::Triangle;
        case QCPScatterStyle::ScatterShape::ssDiamond:
            return GraphMarkerShape::Diamond;
        case QCPScatterStyle::ScatterShape::ssStar:
            return GraphMarkerShape::Star;
        case QCPScatterStyle::ScatterShape::ssPlus:
            return GraphMarkerShape::Plus;
        case QCPScatterStyle::ScatterShape::ssCross:
            return GraphMarkerShape::Cross;
        case QCPScatterStyle::ScatterShape::ssDisc:
            return GraphMarkerShape::FilledCircle;
        case QCPScatterStyle::ScatterShape::ssTriangleInverted:
            return GraphMarkerShape::InvertedTriangle;
        case QCPScatterStyle::ScatterShape::ssCrossSquare:
            return GraphMarkerShape::CrossedSquare;
        case QCPScatterStyle::ScatterShape::ssPlusSquare:
            return GraphMarkerShape::PlusSquare;
        case QCPScatterStyle::ScatterShape::ssCrossCircle:
            return GraphMarkerShape::CrossedCircle;
        case QCPScatterStyle::ScatterShape::ssPlusCircle:
            return GraphMarkerShape::PlusCircle;
        case QCPScatterStyle::ScatterShape::ssCustom:
            return GraphMarkerShape::Custom;
        default:
            return GraphMarkerShape::NoMarker;
    }
}

inline QCPGraph::LineStyle to_qcp(GraphLineStyle style)
{
    switch (style)
    {
        case GraphLineStyle::Line:
            return QCPGraph::LineStyle::lsLine;
        case GraphLineStyle::StepLeft:
            return QCPGraph::LineStyle::lsStepLeft;
        case GraphLineStyle::StepRight:
            return QCPGraph::LineStyle::lsStepRight;
        case GraphLineStyle::StepCenter:
            return QCPGraph::LineStyle::lsStepCenter;
        default:
            return QCPGraph::LineStyle::lsNone;
    }
}

inline GraphLineStyle from_qcp(QCPGraph::LineStyle style)
{
    switch (style)
    {
        case QCPGraph::LineStyle::lsLine:
            return GraphLineStyle::Line;
        case QCPGraph::LineStyle::lsStepLeft:
            return GraphLineStyle::StepLeft;
        case QCPGraph::LineStyle::lsStepRight:
            return GraphLineStyle::StepRight;
        case QCPGraph::LineStyle::lsStepCenter:
            return GraphLineStyle::StepCenter;
        default:
            return GraphLineStyle::NoLine;
    }
}

inline ColorGradient from_qcp(QCPColorGradient::GradientPreset gradient)
{
    switch (gradient)
    {
        case QCPColorGradient::gpGrayscale:
            return ColorGradient::Grayscale;
        case QCPColorGradient::gpHot:
            return ColorGradient::Hot;
        case QCPColorGradient::gpCold:
            return ColorGradient::Cold;
        case QCPColorGradient::gpNight:
            return ColorGradient::Night;
        case QCPColorGradient::gpCandy:
            return ColorGradient::Candy;
        case QCPColorGradient::gpGeography:
            return ColorGradient::Geography;
        case QCPColorGradient::gpIon:
            return ColorGradient::Ion;
        case QCPColorGradient::gpThermal:
            return ColorGradient::Thermal;
        case QCPColorGradient::gpPolar:
            return ColorGradient::Polar;
        case QCPColorGradient::gpSpectrum:
            return ColorGradient::Spectrum;
        case QCPColorGradient::gpJet:
            return ColorGradient::Jet;
        case QCPColorGradient::gpHues:
            return ColorGradient::Hues;
        default:
            return ColorGradient::Jet;
    }
}


inline QCPColorGradient::GradientPreset to_qcp(ColorGradient gradient)
{
    switch (gradient)
    {
        case ColorGradient::Grayscale:
            return QCPColorGradient::gpGrayscale;
        case ColorGradient::Hot:
            return QCPColorGradient::gpHot;
        case ColorGradient::Cold:
            return QCPColorGradient::gpCold;
        case ColorGradient::Night:
            return QCPColorGradient::gpNight;
        case ColorGradient::Candy:
            return QCPColorGradient::gpCandy;
        case ColorGradient::Geography:
            return QCPColorGradient::gpGeography;
        case ColorGradient::Ion:
            return QCPColorGradient::gpIon;
        case ColorGradient::Thermal:
            return QCPColorGradient::gpThermal;
        case ColorGradient::Polar:
            return QCPColorGradient::gpPolar;
        case ColorGradient::Spectrum:
            return QCPColorGradient::gpSpectrum;
        case ColorGradient::Jet:
            return QCPColorGradient::gpJet;
        case ColorGradient::Hues:
            return QCPColorGradient::gpHues;
        default:
            return QCPColorGradient::gpJet;
    }
}


inline LineTermination from_qcp(QCPLineEnding::EndingStyle termination)
{
    switch (termination)
    {
        case QCPLineEnding::esNone:
            return LineTermination::NoneTermination;
        case QCPLineEnding::esFlatArrow:
            return LineTermination::Arrow;
        case QCPLineEnding::esSpikeArrow:
            return LineTermination::SPikeArrow;
        case QCPLineEnding::esLineArrow:
            return LineTermination::LineArrow;
        case QCPLineEnding::esBar:
            return LineTermination::Bar;
        case QCPLineEnding::esDisc:
            return LineTermination::Circle;
        case QCPLineEnding::esDiamond:
            return LineTermination::Diamond;
        case QCPLineEnding::esSquare:
            return LineTermination::Square;
        case QCPLineEnding::esHalfBar:
            return LineTermination::HalfBar;
        case QCPLineEnding::esSkewedBar:
            return LineTermination::SkewedBar;
        default:
            return LineTermination::NoneTermination;
    }
}

inline QCPLineEnding::EndingStyle to_qcp(LineTermination termination)
{
    switch (termination)
    {
        case LineTermination::Arrow:
            return QCPLineEnding::esFlatArrow;
        case LineTermination::SPikeArrow:
            return QCPLineEnding::esSpikeArrow;
        case LineTermination::LineArrow:
            return QCPLineEnding::esLineArrow;
        case LineTermination::Bar:
            return QCPLineEnding::esBar;
        case LineTermination::Circle:
            return QCPLineEnding::esDisc;
        case LineTermination::Diamond:
            return QCPLineEnding::esDiamond;
        case LineTermination::Square:
            return QCPLineEnding::esSquare;
        case LineTermination::HalfBar:
            return QCPLineEnding::esHalfBar;
        case LineTermination::SkewedBar:
            return QCPLineEnding::esSkewedBar;
        default:
            return QCPLineEnding::esNone;
    }
}

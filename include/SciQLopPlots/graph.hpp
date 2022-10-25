/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
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

#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QWheelEvent>
#include <QWidget>

#include "SciQLopPlots/Qt/Events/Keyboard.hpp"
#include "SciQLopPlots/Qt/Events/Mouse.hpp"
#include "SciQLopPlots/Qt/Events/Wheel.hpp"

#include "SciQLopPlots/Interfaces/IGraph.hpp"

#include "SciQLopPlots/Qt/QCustomPlot/QCustomPlotWrapper.hpp"
#include "SciQLopPlots/axis_range.hpp"
#include "SciQLopPlots/enums.hpp"

namespace SciQLopPlots
{

class LineGraph : public interfaces::ILineGraph
{
    Q_OBJECT
    int m_index;

public:
    Q_SIGNAL void setdata(int index, const std::vector<double>& x, const std::vector<double>& y);

    LineGraph(int index, QObject* parent = nullptr)
            : interfaces::ILineGraph(parent), m_index { index }
    {
    }
    virtual void plot(const std::vector<double>& x, const std::vector<double>& y);
};

class MultiLineGraph : public interfaces::IMultiLineGraph
{
    Q_OBJECT
    std::vector<int> m_indexes;

public:
    Q_SIGNAL void setdata(
        std::vector<int> indexes, const std::vector<double>& x, const std::vector<double>& y);

    MultiLineGraph(std::vector<int> indexes, QObject* parent = nullptr)
            : interfaces::IMultiLineGraph(parent), m_indexes { indexes }
    {
    }
    virtual void plot(const std::vector<double>& x, const std::vector<double>& y);
};

class ColorMapGraph : public interfaces::IColorMapGraph
{
    Q_OBJECT

public:
    ColorMapGraph(QObject* parent = nullptr) : interfaces::IColorMapGraph(parent) { }
    virtual void plot(
        const std::vector<double>& x, const std::vector<double>& y, const std::vector<double> z);
};

}

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
#include "SciQLopPlots/Interfaces/IPlotWidget.hpp"

#include "SciQLopPlots/Qt/QCustomPlot/QCustomPlotWrapper.hpp"
#include "SciQLopPlots/axis_range.hpp"
#include "SciQLopPlots/enums.hpp"

namespace SciQLopPlots
{

class LineGraph : public interfaces::ILineGraph
{
    Q_OBJECT
    int m_index;

    interfaces::IPlotWidget* m_plot_widget(){return qobject_cast<interfaces::IPlotWidget*>(this->parent());}
public:

    LineGraph(int index, QObject* parent = nullptr)
            : interfaces::ILineGraph(parent), m_index { index }
    {
    }
protected:
    void plot_ptr(double* x, double* y, std::size_t x_size, std::size_t y_size);
};

class MultiLineGraph : public interfaces::IMultiLineGraph
{
    Q_OBJECT
    std::vector<int> m_indexes;
interfaces::IPlotWidget* m_plot_widget(){return qobject_cast<interfaces::IPlotWidget*>(this->parent());}
public:

    MultiLineGraph(std::vector<int> indexes, interfaces::IPlotWidget* parent = nullptr)
            : interfaces::IMultiLineGraph(parent), m_indexes { indexes }
    {
    }

protected:
    void plot_ptr(double* x, double* y, std::size_t x_size, std::size_t y_size, enums::DataOrder order = enums::DataOrder::x_first);
};

class ColorMapGraph : public interfaces::IColorMapGraph
{
    Q_OBJECT
interfaces::IPlotWidget* m_plot_widget(){return qobject_cast<interfaces::IPlotWidget*>(this->parent());}
public:
    ColorMapGraph(QObject* parent = nullptr) : interfaces::IColorMapGraph(parent) { }
    virtual void plot(
        const std::vector<double>& x, const std::vector<double>& y, const std::vector<double> z);
};

}

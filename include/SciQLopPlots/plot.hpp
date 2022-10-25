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

#include "SciQLopPlots/Interfaces/IPlotWidget.hpp"

#include "SciQLopPlots/Qt/QCustomPlot/QCustomPlotWrapper.hpp"
#include "SciQLopPlots/axis_range.hpp"
#include "SciQLopPlots/enums.hpp"
#include "SciQLopPlots/graph.hpp"

namespace SciQLopPlots
{

/*
 * expected feature list:
 * - mouse wheel scroll/zoom
 * - box zoom
 * - mouse drag scroll
 * - key scroll/zoom
 * - multi-graph sync
 * - graph dragn'grop
 * - data tooltip
 * - basic plot customization, color/style/scales
 * - x axis sharing and x axis sticky
 * - time interval selection boxes
 */

class PlotWidget : public interfaces::IPlotWidget
{
    Q_OBJECT

    void notifyRangeChanged(const axis::range& range, Qt::Orientation orientation) noexcept
    {
        if (orientation == Qt::Horizontal)
        {
            emit xRangeChanged(range);
        }
        else
        {
            emit yRangeChanged(range);
        }
    }

protected:
    QCPWrappers::QCustomPlotWrapper* m_plot;

public:
    explicit PlotWidget(QWidget* parent = nullptr)
            : interfaces::IPlotWidget(parent)
            , m_plot { new QCPWrappers::QCustomPlotWrapper { this } }
    {
        setContentsMargins(0, 0, 0, 0);
        setWidget(m_plot);
        layout()->setSpacing(0);
        layout()->setContentsMargins(0, 0, 0, 0);
        this->setFocusPolicy(Qt::WheelFocus);
        this->setMouseTracking(true);
        m_plot->setAttribute(Qt::WA_TransparentForMouseEvents);
        connect(
            m_plot, &QCPWrappers::QCustomPlotWrapper::dataChanged, this, &PlotWidget::dataChanged);
    }
    ~PlotWidget() { }

    inline double map_pixels_to_data_coordinates(double px, enums::Axis axis) const final
    {
        if (axis == enums::Axis::x)
            return m_plot->pixelToCoord(px, Qt::Horizontal);
        if (axis == enums::Axis::y)
            return m_plot->pixelToCoord(px, Qt::Vertical);
        return nan("");
    }

    inline void set_range(const axis::range& range, enums::Axis axis) final
    {
        switch (axis)
        {
            case enums::Axis::x:
                setXRange(range);
                break;
            case enums::Axis::y:
                setYRange(range);
                break;
            case enums::Axis::z:
                break;
            default:
                break;
        }
    }

    inline void set_range(const axis::range& x_range, const axis::range& y_range) final
    {
        setXRange(x_range);
        setYRange(y_range);
    }

    inline axis::range range(enums::Axis axis) const final
    {
        switch (axis)
        {
            case enums::Axis::x:
                return m_plot->xRange();
                break;
            case enums::Axis::y:
                return m_plot->yRange();
                break;
            case enums::Axis::z:
                return m_plot->zRange();
                break;
            default:
                throw "Unexpected axis";
                break;
        }
    }

    inline void autoScaleY() override { m_plot->autoScaleY(); }

    inline axis::range xRange() { return m_plot->xRange(); }
    inline axis::range yRange() { return m_plot->yRange(); }

    inline void setXRange(const axis::range& range) override
    {
        if (range != xRange())
        {
            m_plot->setXRange(range);
            notifyRangeChanged(range, Qt::Horizontal);
        }
    }

    inline void setYRange(const axis::range& range) override
    {
        if (range != yRange())
        {
            m_plot->setYRange(range);
            notifyRangeChanged(range, Qt::Vertical);
        }
    }

    inline void setRange(const axis::range& range, Qt::Orientation orientation)
    {
        if (orientation == Qt::Horizontal)
            return setXRange(range);
        else
            return setYRange(range);
    }

    inline void plot(int graphIdex, const std::vector<double>& x, const std::vector<double>& y)
    {
        m_plot->plot(graphIdex, x, y);
    }

    inline void plot(const std::vector<int> graphIdexes, const std::vector<double>& x,
        const std::vector<double>& y)
    {
        m_plot->plot(graphIdexes, x, y);
    }

    inline void plot(int graphIdex, const std::vector<double>& x, const std::vector<double>& y,
        const std::vector<double>& z)
    {
        m_plot->plot(graphIdex, x, y, z);
    }

    inline void showXAxis(bool show) override { m_plot->xAxis->setVisible(show); }

    inline void replot(int ms) override { m_plot->replot(ms); }

    virtual LineGraph* addLineGraph(const QColor& color) override;
    virtual MultiLineGraph* addMultiLineGraph(const std::vector<QColor>& colors) override;
    virtual ColorMapGraph* addColorMapGraph() override;

    QCPWrappers::QCustomPlotWrapper* handle() { return this->m_plot; }

    virtual interfaces::GraphicObject* graphicObjectAt(const QPoint& position) override
    {
        return graphic_objects.graphicObjectAt({ position.x(), position.y() });
    }

    virtual interfaces::GraphicObject* nextGraphicObjectAt(
        const QPoint& position, interfaces::GraphicObject* current) override
    {
        return graphic_objects.nextGraphicObjectAt({ position.x(), position.y() }, current);
    }
};

}

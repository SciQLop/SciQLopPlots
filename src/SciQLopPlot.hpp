/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2020, Plasma Physics Laboratory - CNRS
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

#include "IPlotWidget.hpp"

#include <types/detectors.hpp>

#include <channels/channels.hpp>

#include <QObject>
#include <QWidget>

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

HAS_METHOD(has_zoom1, zoom, double, Qt::Orientation);
HAS_METHOD(has_zoom2, zoom, double, double, Qt::Orientation);
HAS_METHOD(has_move, move, double, Qt::Orientation);
HAS_METHOD(has_xRange, xRange);
HAS_METHOD(has_setXRange, setXRange, AxisRange);
HAS_METHOD(has_setYRange, setYRange, AxisRange);


template <typename PlotImpl>
class PlotWidget : public IPlotWidget
{
    static_assert(has_zoom1_v<PlotImpl>, "PlotImpl is missing zoom method.");
    static_assert(has_zoom2_v<PlotImpl>, "PlotImpl is missing zoom method.");
    static_assert(has_move_v<PlotImpl>, "PlotImpl is missing move method.");
    static_assert(has_xRange_v<PlotImpl>, "PlotImpl is missing xRange method.");
    static_assert(has_setXRange_v<PlotImpl>, "PlotImpl is missing setXRange method.");
    static_assert(has_setYRange_v<PlotImpl>, "PlotImpl is missing setYRange method.");


protected:
    PlotImpl* m_plot;
    std::mutex m_plot_mutex;

public:
    PlotWidget(QWidget* parent = nullptr) : IPlotWidget { parent }, m_plot { new PlotImpl { this } }
    {
        setWidget(m_plot);
        this->setFocusPolicy(Qt::WheelFocus);
        this->setMouseTracking(true);
        m_plot->setAttribute(Qt::WA_TransparentForMouseEvents);
        connect(m_plot, &PlotImpl::dataChanged,this, &PlotWidget::dataChanged);
    }

    ~PlotWidget()
    {
        emit closed();
    }

    inline void zoom(double factor, Qt::Orientation orientation = Qt::Horizontal) override
    {
        m_plot->zoom(factor, orientation);
        if (orientation == Qt::Horizontal)
            emit xRangeChanged(m_plot->xRange());
    }

    inline void zoom(
        double factor, double center, Qt::Orientation orientation = Qt::Horizontal) override
    {
        m_plot->zoom(factor, center, orientation);
        if (orientation == Qt::Horizontal)
            emit xRangeChanged(m_plot->xRange());
    }

    inline void move(double factor, Qt::Orientation orientation) override
    {
        m_plot->move(factor, orientation);
        if (orientation == Qt::Horizontal)
            emit xRangeChanged(m_plot->xRange());
    }

    inline void move(double dx, double dy) override
    {
        m_plot->move(dx, dy);
        if (dx != 0.)
            emit xRangeChanged(m_plot->xRange());
        if (dy != 0.)
            emit yRangeChanged(m_plot->yRange());
    }

    inline void autoScaleY() override { m_plot->autoScaleY(); }

    inline void setXRange(const AxisRange& range) override
    {
        m_plot->setXRange(range);
        emit xRangeChanged(range);
    }

    inline void setYRange(const AxisRange& range) override
    {
        m_plot->setYRange(range);
        emit yRangeChanged(range);
    }

    inline int addGraph(QColor color = Qt::blue) override
    {
        return m_plot->addGraph(color);
    }

    template <typename data_t>
    void plot(int graphIdex, const data_t& data)
    {
        m_plot->plot(graphIdex,data);
    }

protected:
    /*void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;*/
};

template <typename plot_t>
void zoom(plot_t& plot, double factor);

template <typename plot_t, typename data_t>
inline void plot(plot_t& plot, int graphIdex, const data_t& data)
{
    plot.plot(graphIdex,data);
}

}

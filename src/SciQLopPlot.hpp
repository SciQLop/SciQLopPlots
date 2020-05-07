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

#include <types/detectors.hpp>

#include <QObject>
#include <QWidget>
#include <plottables/plottable-graph.h>
#include <qcp.h>

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

class IPlotWidget : public QWidget
{
    Q_OBJECT
public:
    IPlotWidget(QWidget* parent = nullptr) : QWidget(parent) { }
    Q_SIGNAL void xRangeChanged(double xStart, double xEnd);
    virtual void zoom(double factor, Qt::Orientation orientation = Qt::Horizontal) = 0;
    virtual void zoom(
        double factor, double center, Qt::Orientation orientation = Qt::Horizontal)
        = 0;
    virtual void move(double factor, Qt::Orientation orientation) = 0;

protected:
    void setWidget(QWidget* widget);
    void wheelEvent(QWheelEvent* event) override;
};

HAS_METHOD(has_zoom1, zoom, double, Qt::Orientation);
HAS_METHOD(has_zoom2, zoom, double, double, Qt::Orientation);
HAS_METHOD(has_move, move, double, Qt::Orientation);

template <typename PlotImpl>
class PlotWidget : public IPlotWidget
{
    static_assert(has_zoom1_v<PlotImpl>, "missing zoom method.");
    static_assert(has_zoom2_v<PlotImpl>, "missing zoom method.");
    static_assert(has_move_v<PlotImpl>, "missing move method.");

protected:
    PlotImpl* m_plot;

public:
    PlotWidget(QWidget* parent = nullptr) : IPlotWidget { parent }, m_plot { new PlotImpl { this } }
    {
        setWidget(m_plot);
        this->setFocusPolicy(Qt::WheelFocus);
        this->setMouseTracking(true);
        m_plot->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    inline void zoom(double factor, Qt::Orientation orientation = Qt::Horizontal) override
    {
        m_plot->zoom(factor, orientation);
    }

    inline void zoom(
        double factor, double center, Qt::Orientation orientation = Qt::Horizontal) override
    {
        m_plot->zoom(factor, center,orientation);
    }

    inline void move(double factor, Qt::Orientation orientation) override
    {
        m_plot->move(factor, orientation);
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

template <typename plot_t, typename data_t>
void plot(plot_t& plot, const data_t& data);

template <typename plot_t>
void zoom(plot_t& plot, double factor);


class QCustomPlotWrapper : public QCustomPlot
{
    Q_OBJECT
public:
    QCustomPlotWrapper(QWidget* parent = nullptr) : QCustomPlot { parent }
    {
        setPlottingHint(QCP::phFastPolylines, true);
    }

    inline void zoom(double factor, Qt::Orientation orientation = Qt::Horizontal)
    {
        QCPAxis* axis = axisRect()->rangeZoomAxis(orientation);
        axis->scaleRange(factor);
        replot(QCustomPlot::rpQueuedReplot);
    }

    inline void zoom(
        double factor, double center, Qt::Orientation orientation = Qt::Horizontal)
    {
        QCPAxis* axis = axisRect()->rangeZoomAxis(orientation);
        axis->scaleRange(factor, axis->pixelToCoord(center));
        replot(QCustomPlot::rpQueuedReplot);
    }

    inline void move(double factor, Qt::Orientation orientation)
    {
        auto axis = axisRect()->rangeDragAxis(orientation);
        auto distance = abs(axis->range().upper - axis->range().lower)*factor;
        axis->setRange(QCPRange(axis->range().lower + distance, axis->range().upper + distance));
        replot(QCustomPlot::rpQueuedReplot);
    }
};

}

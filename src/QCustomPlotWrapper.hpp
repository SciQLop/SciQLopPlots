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

#include <QObject>
#include <QTimer>
#include <QWidget>

#include <qcp.h>

#include "IPlotWidget.hpp"
#include "SciQLopPlot.hpp"

namespace SciQLopPlots
{

class QCustomPlotWrapper : public QCustomPlot
{
    Q_OBJECT
    QTimer* p_refresh_timer;

public:
    QCustomPlotWrapper(QWidget* parent = nullptr) : QCustomPlot(parent)
    {
        p_refresh_timer = new QTimer{this};
        p_refresh_timer->setSingleShot(true);
        setPlottingHint(QCP::phFastPolylines, true);
        connect(this, &QCustomPlotWrapper::_plot, this, &QCustomPlotWrapper::_plot_slt,
            Qt::AutoConnection);
        connect(this->p_refresh_timer, &QTimer::timeout,
            [this]() { QCustomPlot::replot(QCustomPlot::rpQueuedReplot); });
        // quick hard coded path, might be abstrated like AxisRenderingUtils.cpp in sciqlop code
        auto dateTicker = QSharedPointer<QCPAxisTickerDateTime>::create();
        dateTicker->setDateTimeFormat("yyyy/MM/dd \nhh:mm:ss");
        dateTicker->setDateTimeSpec(Qt::UTC);
        xAxis->setTicker(dateTicker);
        removeAllMargins(this);
        plotLayout()->setMargins(QMargins { 0, 0, 0, 0 });
        plotLayout()->setRowSpacing(0);
        for (auto rect : axisRects())
        {
            rect->setMargins(QMargins { 0, 0, 0, 0 });
        }
    }

    inline double pixelToCoord(double pixelCoord, Qt::Orientation orientation)
    {
        auto axis = axisRect()->rangeDragAxis(orientation);
        return axis->pixelToCoord(pixelCoord);
    }

    inline double pixelToCoord(QPoint pixelCoord, Qt::Orientation orientation)
    {
        if (orientation == Qt::Vertical)
            return pixelToCoord(pixelCoord.y(), Qt::Vertical);
        return pixelToCoord(pixelCoord.x(), Qt::Horizontal);
    }


    inline void autoScaleY()
    {
        yAxis->rescale(true);
        QCustomPlot::replot(QCustomPlot::rpQueuedReplot);
    }

    inline AxisRange xRange() { return { xAxis->range().lower, xAxis->range().upper }; }
    inline AxisRange yRange() { return { yAxis->range().lower, yAxis->range().upper }; }

    inline void setXRange(const AxisRange& range)
    {
        xAxis->setRange({ range.first, range.second });
        QCustomPlot::replot(QCustomPlot::rpQueuedReplot);
    }

    inline void setYRange(const AxisRange& range)
    {
        yAxis->setRange({ range.first, range.second });
        QCustomPlot::replot(QCustomPlot::rpQueuedReplot);
    }

    inline int addGraph(QColor color = Qt::blue)
    {
        auto graph = QCustomPlot::addGraph();
        graph->setAdaptiveSampling(true);
        auto pen = graph->pen();
        pen.setColor(color);
        graph->setPen(pen);
        return graphCount() - 1;
    }

    inline bool addColorMap()
    {
        return false;
    }

    using data_t = std::pair<std::vector<double>, std::vector<double>>;

    inline void plot(int graphIdex, const data_t& data)
    {
        plot(graphIdex, data.first, data.second);
    }

    inline void plot(int graphIndex, const std::vector<double>& x, const std::vector<double>& y)
    {
        QVector<QCPGraphData> data;
        std::transform(std::cbegin(x), std::cend(x), std::cbegin(y), std::back_inserter(data),
            [](double x, double y) {
                return QCPGraphData { x, y };
            });
        emit _plot(graphIndex, data);
    }

    Q_SIGNAL void dataChanged();

    inline void replot(int ms){this->p_refresh_timer->start(ms);}

private:
    Q_SIGNAL void _plot(int graphIndex, const QVector<QCPGraphData>& data);
    Q_SLOT void _plot_slt(int graphIndex, const QVector<QCPGraphData>& data);
};

using SciQLopPlot = PlotWidget<QCustomPlotWrapper>;


}

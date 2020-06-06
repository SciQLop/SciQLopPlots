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
#include <QWidget>

#include <qcp.h>

#include "IPlotWidget.hpp"

namespace SciQLopPlots
{

class QCustomPlotWrapper : public QCustomPlot
{
    Q_OBJECT
public:
    QCustomPlotWrapper(QWidget* parent = nullptr) : QCustomPlot(parent)
    {
        setPlottingHint(QCP::phFastPolylines, true);
    }

    inline void zoom(double factor, Qt::Orientation orientation = Qt::Horizontal)
    {
        QCPAxis* axis = axisRect()->rangeZoomAxis(orientation);
        axis->scaleRange(factor);
        replot(QCustomPlot::rpQueuedReplot);
    }

    inline void zoom(double factor, double center, Qt::Orientation orientation = Qt::Horizontal)
    {
        QCPAxis* axis = axisRect()->rangeZoomAxis(orientation);
        axis->scaleRange(factor, axis->pixelToCoord(center));
        replot(QCustomPlot::rpQueuedReplot);
    }

    inline void move(double factor, Qt::Orientation orientation)
    {
        auto axis = axisRect()->rangeDragAxis(orientation);
        auto distance = abs(axis->range().upper - axis->range().lower) * factor;
        axis->setRange(QCPRange(axis->range().lower + distance, axis->range().upper + distance));
        replot(QCustomPlot::rpQueuedReplot);
    }

    inline void autoScaleY()
    {
        yAxis->rescale(true);
        replot(QCustomPlot::rpQueuedReplot);
    }

    inline AxisRange xRange() { return { xAxis->range().lower, xAxis->range().upper }; }

    inline void setXRange(const AxisRange& range)
    {
        xAxis->setRange({ range.first, range.second });
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

    inline void plot(int graphIndex, const std::vector<double>& x, const std::vector<double>& y)
    {
        QVector<QCPGraphData> data;
        std::transform(std::cbegin(x), std::cend(x), std::cbegin(y), std::back_inserter(data),
            [](double x, double y) {
                return QCPGraphData { x, y };
            });
        graph(graphIndex)->data()->set(data, true);
    }
};
}

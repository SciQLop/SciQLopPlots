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

#include <cpp_utils/containers/algorithms.hpp>

#include <iostream>
#include <qcp.h>

#include "SciQLopPlots/Interfaces/GraphicObjects/GraphicObject.hpp"
#include "SciQLopPlots/Interfaces/IPlotWidget.hpp"

#include "SciQLopPlots/axis_range.hpp"


namespace SciQLopPlots::QCPWrappers
{

class QCustomPlotWrapper : public QCustomPlot
{
    Q_OBJECT
    QTimer* p_refresh_timer;

public:
    QCustomPlotWrapper(QWidget* parent = nullptr) : QCustomPlot(parent)
    {
        p_refresh_timer = new QTimer { this };
        p_refresh_timer->setSingleShot(true);
        setPlottingHint(QCP::phFastPolylines, true);
        setInteractions(QCP::iSelectPlottables | QCP::iSelectItems);
        connect(this, QOverload<int, const QVector<QCPGraphData>&>::of(&QCustomPlotWrapper::_plot),
            this, QOverload<int, const QVector<QCPGraphData>&>::of(&QCustomPlotWrapper::_plot_slt),
            Qt::QueuedConnection);
        connect(this, QOverload<QCPColorMapData*>::of(&QCustomPlotWrapper::_plot), this,
            QOverload<QCPColorMapData*>::of(&QCustomPlotWrapper::_plot_slt), Qt::QueuedConnection);
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
        if (m_colormap)
            m_colormap->rescaleDataRange();
        QCustomPlot::replot(QCustomPlot::rpQueuedReplot);
    }

    inline axis::range xRange() { return { xAxis->range().lower, xAxis->range().upper }; }
    inline axis::range yRange() { return { yAxis->range().lower, yAxis->range().upper }; }
    inline axis::range zRange() { return { 0., 0. }; } // TODO

    inline void setXRange(const axis::range& range)
    {
        xAxis->setRange({ range.first, range.second });
        QCustomPlot::replot(QCustomPlot::rpQueuedReplot);
    }

    inline void setYRange(const axis::range& range)
    {
        yAxis->setRange({ range.first, range.second });
        QCustomPlot::replot(QCustomPlot::rpQueuedReplot);
    }

    inline void setZRange(const axis::range& range)
    {
        // TODO
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

    inline int addColorMap()
    {
        if (!m_colormap)
        {
            m_colormap = new QCPColorMap(this->xAxis, this->yAxis);
            return 0;
        }
        return -1;
    }

    using data_t = std::pair<std::vector<double>, std::vector<double>>;
    using data2d_t = std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>;

    inline void plot(int graphIdex, const data_t& data)
    {
        plot(graphIdex, data.first, data.second);
    }

    inline void plot(const std::vector<int> graphIdexes, const data_t& data)
    {
        plot(graphIdexes, data.first, data.second);
    }

    inline void plot(const std::vector<int> graphIdexes, const data2d_t& data)
    {
        assert(std::size(graphIdexes) == 1);
        plot(graphIdexes[0], data);
    }

    inline void plot(int graphIndex, const data2d_t& data)
    {
        plot(graphIndex, std::get<0>(data), std::get<1>(data), std::get<2>(data));
    }

    inline void plot(int graphIndex, const std::vector<double>& x, const std::vector<double>& y)
    {
        QVector<QCPGraphData> data(std::size(x));
        std::transform(std::cbegin(x), std::cend(x), std::cbegin(y), std::begin(data),
            [](double x, double y) {
                return QCPGraphData { x, y };
            });
        emit _plot(graphIndex, data);
    }

    inline void plot(const std::vector<int> graphIdexes, const std::vector<double>& x,
        const std::vector<double>& y)
    {
        if (std::size(y) / std::size(graphIdexes) == std::size(x))
        {
            auto y_it = std::cbegin(y);
            for (auto graphIndex : graphIdexes)
            {
                QVector<QCPGraphData> data(std::size(x));
                std::transform(std::cbegin(x), std::cend(x), y_it, std::begin(data),
                    [](double x, double y) {
                        return QCPGraphData { x, y };
                    });
                emit _plot(graphIndex, data);
                y_it += std::size(x);
            }
        }
        else
        {
            std::cerr << "Wrong data shape: " << std::endl
                      << "std::size(y)=" << std::size(y) << std::endl
                      << "std::size(graphIdexes)=" << std::size(graphIdexes) << std::endl
                      << "std::size(x)=" << std::size(x) << std::endl;
        }
    }

    inline void plot(int graphIndex, const std::vector<double>& x, const std::vector<double>& y,
        const std::vector<double>& z)
    {
        using namespace cpp_utils;
        if (std::size(x) && std::size(y) && std::size(z))
        {
            auto data = new QCPColorMapData(std::size(x), std::size(y),
                { *containers::min(x), *containers::max(x) },
                { *containers::min(y), *containers::max(y) });
            auto it = std::cbegin(z);
            for (const auto i : x)
            {
                for (const auto j : y)
                {
                    if (it != std::cend(z))
                    {
                        data->setData(i, j, *it);
                        it++;
                    }
                }
            }
            emit _plot(data);
        }
    }

    Q_SIGNAL void dataChanged();

    inline void replot(int ms) { this->p_refresh_timer->start(ms); }

    interfaces::GraphicObject* graphicObjectAt(const QPoint& position) { return nullptr; }


private:
    Q_SIGNAL void _plot(int graphIndex, const QVector<QCPGraphData>& data);
    Q_SIGNAL void _plot(QCPColorMapData* data);
    Q_SLOT void _plot_slt(int graphIndex, const QVector<QCPGraphData>& data);
    Q_SLOT void _plot_slt(QCPColorMapData* data);
    QCPColorMap* m_colormap = nullptr;
};


}

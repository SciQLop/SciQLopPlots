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
#include <QSharedDataPointer>
#include <QTimer>
#include <QWidget>

#include <cpp_utils/containers/algorithms.hpp>

#include <iostream>
#include <list>
#include <qcp.h>
#include <vector>

#include "SciQLopPlots/Interfaces/GraphicObjects/GraphicObject.hpp"
#include "SciQLopPlots/Interfaces/IPlotWidget.hpp"

#include "SciQLopPlots/axis_range.hpp"


namespace SciQLopPlots::QCPWrappers
{

template <std::size_t dest_size>
inline QVector<QCPGraphData>* resample(
    const double* x, const double* y, std::size_t x_size, std::size_t y_size, const int y_incr = 1)
{
    auto dx = (x[x_size - 1] - x[0]) / dest_size;
    auto data = new QVector<QCPGraphData>(dest_size);
    const double* current_x = x;
    const double* current_y_it = y;
    for (auto bucket = 0UL; bucket < dest_size; bucket++)
    {
        double bucket_max_x = std::min(x[0] + (bucket + 1) * dx, x[x_size - 1]);
        if ((bucket & 1) == 0)
        {
            double max_value = std::nan("");
            while (*current_x < bucket_max_x)
            {
                max_value = std::fmax(max_value, *current_y_it);
                current_y_it += y_incr;
                current_x++;
            }
            (*data)[bucket] = QCPGraphData { x[0] + bucket * dx, max_value };
        }
        else
        {
            double min_value = std::nan("");
            while (*current_x < bucket_max_x)
            {
                min_value = std::fmin(min_value, *current_y_it);
                current_y_it += y_incr;
                current_x++;
            }
            (*data)[bucket] = QCPGraphData { x[0] + bucket * dx, min_value };
        }
    }
    return data;
}

inline QVector<QCPGraphData>* copy_data(
    const double* x, const double* y, std::size_t x_size, std::size_t y_size, const int y_incr = 1)
{
    auto data = new QVector<QCPGraphData>(x_size);
    const double* current_y_it = y;
    for (auto i = 0UL; i < x_size; i++)
    {
        (*data)[i] = QCPGraphData { x[i], *current_y_it };
        current_y_it += y_incr;
    }
    return data;
}

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
        connect(this, QOverload<int, QVector<QCPGraphData>*>::of(&QCustomPlotWrapper::_plot), this,
            QOverload<int, QVector<QCPGraphData>*>::of(&QCustomPlotWrapper::_plot_slt),
            Qt::QueuedConnection);
        connect(this,
            QOverload<std::vector<int>, QVector<QVector<QCPGraphData>*>>::of(
                &QCustomPlotWrapper::_plot),
            this,
            QOverload<std::vector<int>, QVector<QVector<QCPGraphData>*>>::of(
                &QCustomPlotWrapper::_plot_slt),
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
        // graph->setAdaptiveSampling(true);
        auto pen = graph->pen();
        pen.setColor(color);
        graph->setPen(pen);
        return graphCount() - 1;
    }

    inline void setGraphColor(int graphIndex, QColor color)
    {
        auto graph = this->graph(graphIndex);
        auto pen = graph->pen();
        pen.setColor(color);
        graph->setPen(pen);
    }

    inline QColor graphColor(int graphIndex) { return this->graph(graphIndex)->pen().color(); }

    inline int addColorMap()
    {
        if (!m_colormap)
        {
            m_colormap = new QCPColorMap(this->xAxis, this->yAxis);
            m_colormap->setDataScaleType(QCPAxis::stLogarithmic);
            auto scale = QCPColorGradient(QCPColorGradient::gpJet);
            scale.setNanColor(QColor(0, 0, 0, 0));
            m_colormap->setGradient(scale);
            return 0;
        }
        return -1;
    }

    inline void plot(int graphIndex, const std::vector<double>& x, const std::vector<double>& y)
    {
        this->plot(graphIndex, x.data(), y.data(), std::size(x), std::size(y));
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

    inline void plot(
        int graphIndex, const double* x, const double* y, std::size_t x_size, std::size_t y_size)
    {
        if (x_size < 10000)
        {
            auto data = copy_data(x, y, x_size, y_size, 1);
            emit _plot(graphIndex, data);
        }
        else
        {
            auto data = resample<10000>(x, y, x_size, y_size, 1);
            emit _plot(graphIndex, data);
        }
    }

    inline void plot(std::vector<int> graphIndexes, const double* x, const double* y,
        std::size_t x_size, std::size_t y_size, enums::DataOrder order)
    {
        if (y_size / std::size(graphIndexes) == x_size)
        {
            auto datas = QVector<QVector<QCPGraphData>*>(std::size(graphIndexes));
            for (auto dataIndex = 0UL; dataIndex < std::size(graphIndexes); dataIndex++)
            {
                if (order == enums::DataOrder::x_first)
                {
                    if (x_size < 10000)
                    {
                        datas[dataIndex] = copy_data(x, y + x_size * dataIndex, x_size, y_size, 1);
                    }
                    else
                    {
                        datas[dataIndex]
                            = resample<10000>(x, y + x_size * dataIndex, x_size, y_size, 1);
                    }
                }
                else
                {
                    if (x_size < 10000)
                    {
                        datas[dataIndex]
                            = copy_data(x, y + dataIndex, x_size, y_size, std::size(graphIndexes));
                    }
                    else
                    {
                        datas[dataIndex] = resample<10000>(
                            x, y + dataIndex, x_size, y_size, std::size(graphIndexes));
                    }
                }
            }
            emit _plot(graphIndexes, datas);
        }
        else
        {
            std::cerr << "Wrong data shape: " << std::endl
                      << "std::size(y)=" << y_size << std::endl
                      << "std::size(graphIndexes)=" << std::size(graphIndexes) << std::endl
                      << "std::size(x)=" << x_size << std::endl;
        }
    }

    inline void plot(const double* x, const double* y, const double* z, std::size_t x_size,
        std::size_t y_size, std::size_t z_size)
    {
        if (x_size * y_size == z_size)
        {
            using namespace cpp_utils;
            auto data = new QCPColorMapData(x_size, y_size,
                { *std::min_element(x, x + x_size), *std::max_element(x, x + x_size) },
                { *std::min_element(y, y + y_size), *std::max_element(y, y + y_size) });
            auto z_it = z;
            for (auto i = 0UL; i < x_size; i++)
            {
                for (auto j = 0UL; j < y_size; j++)
                {
                    data->setData(x[i], y[j], *z_it);
                    z_it++;
                }
            }
            emit _plot(data);
        }
        else
        {
            std::cerr << "Wrong data shape: " << std::endl
                      << "std::size(z)=" << z_size << std::endl
                      << "std::size(y)=" << y_size << std::endl
                      << "std::size(x)=" << x_size << std::endl;
        }
    }

    Q_SIGNAL void dataChanged();

    inline void replot(int ms) { this->p_refresh_timer->start(ms); }

    interfaces::GraphicObject* graphicObjectAt(const QPoint& position) { return nullptr; }

private:
    Q_SIGNAL void _plot(int graphIndex, QVector<QCPGraphData>* data);
    Q_SIGNAL void _plot(std::vector<int> graphIndexes, QVector<QVector<QCPGraphData>*> data);
    Q_SIGNAL void _plot(QCPColorMapData* data);
    Q_SLOT void _plot_slt(int graphIndex, QVector<QCPGraphData>* data);
    Q_SLOT void _plot_slt(std::vector<int> graphIndexes, QVector<QVector<QCPGraphData>*> data);
    Q_SLOT void _plot_slt(QCPColorMapData* data);
    QCPColorMap* m_colormap = nullptr;
};


}

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

#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QWheelEvent>
#include <QWidget>

#include "SciQLopPlots/Qt/Events/Keyboard.hpp"
#include "SciQLopPlots/Qt/Events/Mouse.hpp"
#include "SciQLopPlots/Qt/Events/Wheel.hpp"

#include "SciQLopPlots/axis_range.hpp"
#include "SciQLopPlots/enums.hpp"

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

namespace interfaces
{
    HAS_METHOD(has_xRange, xRange);
    HAS_METHOD(has_yRange, yRange);
    HAS_METHOD(has_setXRange, setXRange, axis::range);
    HAS_METHOD(has_setYRange, setYRange, axis::range);
    HAS_METHOD(has_pixelToCoord, pixelToCoord, double, Qt::Orientation);


    template <typename PlotImpl>
    class PlotWidget : public IPlotWidget
    {
        static_assert(has_pixelToCoord_v<PlotImpl>, "PlotImpl is missing pixelToCoord method.");
        static_assert(has_xRange_v<PlotImpl>, "PlotImpl is missing xRange method.");
        static_assert(has_yRange_v<PlotImpl>, "PlotImpl is missing yRange method.");
        static_assert(has_setXRange_v<PlotImpl>, "PlotImpl is missing setXRange method.");
        static_assert(has_setYRange_v<PlotImpl>, "PlotImpl is missing setYRange method.");

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
        PlotImpl* m_plot;
        std::mutex m_plot_mutex;

    public:
        using plot_impl_t = PlotImpl;

        PlotWidget(QWidget* parent = nullptr)
                : IPlotWidget { parent }, m_plot { new PlotImpl { this } }
        {
            setContentsMargins(0, 0, 0, 0);
            setWidget(m_plot);
            layout()->setSpacing(0);
            layout()->setContentsMargins(0, 0, 0, 0);
            this->setFocusPolicy(Qt::WheelFocus);
            this->setMouseTracking(true);
            m_plot->setAttribute(Qt::WA_TransparentForMouseEvents);
            connect(m_plot, &PlotImpl::dataChanged, this, &PlotWidget::dataChanged);
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

        inline void showXAxis(bool show) override { m_plot->xAxis->setVisible(show); }

        inline void replot(int ms) override { m_plot->replot(ms); }

        inline int addGraph(QColor color = Qt::blue) override { return m_plot->addGraph(color); }

        inline int addColorMap() override { return m_plot->addColorMap(); }

        template <typename data_t>
        void plot(int graphIdex, const data_t& data)
        {
            std::unique_lock<std::mutex> lock(m_plot_mutex);
            m_plot->plot(graphIdex, data);
        }

        plot_impl_t* handle() { return this->m_plot; }

        virtual GraphicObject* graphicObjectAt(const QPoint& position) override
        {
            return graphic_objects.graphicObjectAt({ position.x(), position.y() });
        }

        virtual GraphicObject* nextGraphicObjectAt(
            const QPoint& position, GraphicObject* current) override
        {
            return graphic_objects.nextGraphicObjectAt({ position.x(), position.y() }, current);
        }
    };
}
template <typename plot_t>
void zoom(plot_t& plot, double factor);

template <typename plot_t, typename data_t>
inline void plot(plot_t& plot, int graphIdex, const data_t& data)
{
    plot.plot(graphIdex, data);
}

}

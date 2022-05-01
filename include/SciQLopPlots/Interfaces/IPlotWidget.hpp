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

#include <QLayout>
#include <QObject>
#include <QWidget>

#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "./GraphicObjects/GraphicObject.hpp"
#include "./GraphicObjects/LayeredGraphicObjectCollection.hpp"
#include "SciQLopPlots/axis_range.hpp"

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

inline void removeAllMargins(QWidget* widget)
{
    widget->setContentsMargins(0, 0, 0, 0);
    auto layout = widget->layout();
    if (layout)
    {
        layout->setSpacing(0);
        layout->setMargin(0);
        layout->setContentsMargins(0, 0, 0, 0);
    }
}

namespace interfaces
{
    class IPlotWidget : public QWidget
    {
        Q_OBJECT

        friend GraphicObject;

        inline void registerGraphicObject(interfaces::GraphicObject* go)
        {
            graphic_objects.registerGraphicObject(go);
        }

        inline void registerGraphicObject(interfaces::GraphicObject* go, std::size_t layer)
        {
            graphic_objects.registerGraphicObject(go, layer);
        }

        void removeGraphicObject(interfaces::GraphicObject* go)
        {
            graphic_objects.removeGraphicObject(go);
        }

    public:
        IPlotWidget(QWidget* parent = nullptr) : QWidget(parent) { }

        virtual double map_pixels_to_data_coordinates(double px, enums::Axis axis) const = 0;
        virtual void set_range(const axis::range& range, enums::Axis axis) = 0;
        virtual void set_range(const axis::range& x_range, const axis::range& y_range) = 0;
        virtual axis::range range(enums::Axis axis) const = 0;


        virtual void autoScaleY() = 0;

        virtual int addGraph(QColor color = Qt::blue) = 0;
        virtual int addColorMap() = 0;

        virtual void setXRange(const axis::range& range) = 0;
        virtual void setYRange(const axis::range& range) = 0;

        virtual void showXAxis(bool show) = 0;

        virtual void replot(int ms) = 0;

        Q_SIGNAL void xRangeChanged(axis::range newRange);
        Q_SIGNAL void yRangeChanged(axis::range newRange);
        Q_SIGNAL void dataChanged();
        Q_SIGNAL void closed();

    protected:
        void setWidget(QWidget* widget);
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        /*void enterEvent(QEvent* event) override;
        void leaveEvent(QEvent* event) override;

        void mouseDoubleClickEvent(QMouseEvent* event) override;*/

        virtual GraphicObject* graphicObjectAt(const QPoint& position) = 0;

    protected:
        LayeredGraphicObjectCollection<3> graphic_objects;
        std::optional<QPoint> m_lastMousePress = std::nullopt;
        interfaces::GraphicObject* m_selected_object = nullptr;
        bool m_has_moved_since_pouse_press;
    };
}
}

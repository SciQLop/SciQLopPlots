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
#include "./IGraph.hpp"
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

inline void removeAllMargins(QWidget* widget)
{
    widget->setContentsMargins(0, 0, 0, 0);
    auto layout = widget->layout();
    if (layout)
    {
        layout->setSpacing(0);
        // layout->setMargin(0);
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

        virtual double map_pixels_to_data_coordinates(double px, enums::Axis axis) const;
        virtual void set_range(const axis::range& range, enums::Axis axis);
        virtual void set_range(const axis::range& x_range, const axis::range& y_range);
        virtual axis::range range(enums::Axis axis) const;


        virtual void autoScaleY();

        virtual ILineGraph* addLineGraph(const QColor& color);
        virtual IMultiLineGraph* addMultiLineGraph(const std::vector<QColor>& colors);
        virtual IColorMapGraph* addColorMapGraph();

        virtual void setXRange(const axis::range& range);
        virtual void setYRange(const axis::range& range);

        virtual void showXAxis(bool show);

        virtual void replot(int ms);

        void delete_selected_object();

        Q_SIGNAL void xRangeChanged(axis::range newRange);
        Q_SIGNAL void yRangeChanged(axis::range newRange);
        Q_SIGNAL void dataChanged();
        Q_SIGNAL void closed();

        virtual GraphicObject* graphicObjectAt(const QPoint& position);
        virtual GraphicObject* nextGraphicObjectAt(const QPoint& position, GraphicObject* current);

        inline void setInteractionsMode(enums::IteractionsMode mode) { m_interactions_mode = mode; }
        inline void setObjectFactory(std::shared_ptr<IGraphicObjectFactory>&& factory)
        {
            m_object_factory = std::move(factory);
        }
        inline void setObjectFactory(const std::shared_ptr<IGraphicObjectFactory>& factory)
        {
            m_object_factory = factory;
        }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        inline QPointF mapFromParent(const QPointF& pos) const
        {
            return QWidget::mapFromParent(pos.toPoint());
        }
#endif

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


    protected:
        LayeredGraphicObjectCollection<4> graphic_objects;
        std::optional<QPoint> m_lastMousePress = std::nullopt;
        interfaces::GraphicObject* m_selected_object = nullptr;
        interfaces::GraphicObject* m_prev_selected_object = nullptr;
        bool m_has_moved_since_mouse_press;
        enums::IteractionsMode m_interactions_mode = enums::IteractionsMode::Normal;

        std::shared_ptr<IGraphicObjectFactory> m_object_factory = nullptr;
    };
}
}

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
#include "QCustomPlotWrapper.hpp"
#include "SciQLopPlots/Interfaces/GraphicObjects/GraphicObject.hpp"
#include "SciQLopPlots/Interfaces/GraphicObjects/TimeSpan.hpp"
#include "SciQLopPlots/Interfaces/IPlotWidget.hpp"
#include <QBrush>
#include <qcp.h>

namespace SciQLopPlots::QCPWrappers
{

class QCPTimeSPanBorder : public QCPItemStraightLine, interfaces::GraphicObject
{
    Q_OBJECT

    inline double pixelToXCoord(double px) const
    {
        return parentPlot()->axisRect()->axis(QCPAxis::atBottom)->pixelToCoord(px);
    }

    inline double coordToXPixel(double x) const
    {
        return parentPlot()->axisRect()->axis(QCPAxis::atBottom)->coordToPixel(x);
    }


public:
    inline QCPTimeSPanBorder(SciQLopPlot* plot)
            : QCPItemStraightLine { plot->handle() }, interfaces::GraphicObject { plot, 0 }
    {
        point1->setTypeX(QCPItemPosition::ptAbsolute);
        point2->setTypeY(QCPItemPosition::ptAbsolute);
        point1->setTypeX(QCPItemPosition::ptAbsolute);
        point2->setTypeY(QCPItemPosition::ptAbsolute);
        this->setPen(QPen { QBrush { QColor(0, 255, 255, 255), Qt::SolidPattern }, 3 });
        setLayer("overlay");
    }

    inline void set_anchor(QCPItemAnchor* top_anchor, QCPItemAnchor* botom_anchor)
    {
        this->point1->setParentAnchor(top_anchor);
        this->point2->setParentAnchor(botom_anchor);
    }

    inline virtual view::data_coordinates<2> center() const override
    {

        return { point1->key(), (point1->value() + point2->value()) / 2 };
    }

    inline virtual view::pixel_coordinates<2> pix_center() const override
    {
        return { point1->pixelPosition().x(),
            (point1->pixelPosition().y() + point2->pixelPosition().y()) / 2 };
    }

    inline virtual void move(const view::data_coordinates<2>& delta) override
    {
        const auto dx = delta.component(enums::Axis::x).value;
        emit move_sig(dx);
    }

    inline virtual void move(const view::pixel_coordinates<2>& delta) override
    {
        const auto dx = pixelToXCoord(delta.component(enums::Axis::x).value) - pixelToXCoord(0);
        emit move_sig(dx);
    }

    inline virtual bool contains(const view::data_coordinates<2>& position) const override
    {
        const auto x = position.component(enums::Axis::x).value;
        return contains(view::pixel_coordinates<2> { coordToXPixel(x), 0 });
    }
    inline virtual bool contains(const view::pixel_coordinates<2>& position) const override
    {
        const auto x = position.component(enums::Axis::x).value;
        return x >= (point1->pixelPosition().x() - fmax(pen().widthF(), 10))
            && x <= (point1->pixelPosition().x() + fmax(pen().widthF(), 10));
    }

    inline virtual void set_selected(bool select) override
    {
        this->setSelected(select);
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }

    Q_SIGNAL void move_sig(double dx);
};

class QCPTimeSpan : public QCPItemRect
{
    Q_OBJECT
    inline void set_auto_extend_vertically()
    {
        topLeft->setTypeX(QCPItemPosition::ptPlotCoords);
        topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
        bottomRight->setTypeX(QCPItemPosition::ptPlotCoords);
        bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    }

    inline double pixelToXCoord(double px) const
    {
        return parentPlot()->axisRect()->axis(QCPAxis::atBottom)->pixelToCoord(px);
    }

    QCPTimeSPanBorder left_border, right_border;

    double mouse_event_initial_left_pos, mouse_event_initial_right_pos;

public:
    using plot_t = QCustomPlotWrapper;

    QCPTimeSpan(SciQLopPlot* plot, axis::range time_range)
            : QCPItemRect { plot->handle() }, left_border { plot }, right_border { plot }
    {
        left_border.set_anchor(topLeft, bottomLeft);
        right_border.set_anchor(topRight, bottomRight);
        set_auto_extend_vertically();
        set_range(time_range);
        setLayer("overlay");
        setBrush(QBrush { QColor(0, 255, 0, 40), Qt::SolidPattern });
        this->setPen(QPen { Qt::NoPen });
        setSelectable(true);

        connect(&this->left_border, &QCPTimeSPanBorder::move_sig,
            [this](double dx)
            {
                auto range = this->range();
                set_range({ range.first + dx, range.second });
            });
        connect(&this->right_border, &QCPTimeSPanBorder::move_sig,
            [this](double dx)
            {
                auto range = this->range();
                set_range({ range.first, range.second + dx });
            });
    }

    void set_range(const axis::range& time_range)
    {
        topLeft->setCoords(time_range.first, 0);
        bottomRight->setCoords(time_range.second, 1);
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }

    axis::range range() const { return { topLeft->key(), bottomRight->key() }; };

    view::data_coordinates<2> center() const
    {
        return { (topLeft->key() + bottomRight->key()) / 2.,
            (topLeft->value() + bottomRight->value()) / 2. };
    }

    view::pixel_coordinates<2> pix_center() const
    {
        return { (topLeft->pixelPosition().x() + bottomRight->pixelPosition().x()) / 2.,
            (topLeft->pixelPosition().y() + bottomRight->pixelPosition().y()) / 2. };
    }

    inline void move(double dt) { set_range(range() + dt); }

    inline void move(const view::data_coordinates<2>& delta)
    {
        move(delta.component(enums::Axis::x).value);
    }

    inline void move(const view::pixel_coordinates<2>& delta)
    {
        move(pixelToXCoord(delta.component(enums::Axis::x).value) - pixelToXCoord(0));
    }

    inline bool contains(const view::data_coordinates<2>& position) const
    {
        const auto x = position.component(enums::Axis::x).value;
        const auto y = position.component(enums::Axis::y).value;

        return (x <= bottomRight->key()) && (x >= topLeft->key()) && (y <= topLeft->value())
            && (y >= bottomRight->value());
    }

    inline bool contains(const view::pixel_coordinates<2>& position) const
    {
        const auto x = position.component(enums::Axis::x).value;
        const auto y = position.component(enums::Axis::y).value;

        const auto min_x = topLeft->pixelPosition().x();
        const auto max_x = bottomRight->pixelPosition().x();

        const auto min_y = topLeft->pixelPosition().y();
        const auto max_y = bottomRight->pixelPosition().y();

        return (x <= max_x) && (x >= min_x) && (y <= max_y) && (y >= min_y);
    }

    void set_selected(bool select)
    {
        setSelected(select);
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }
};
using TimeSpan = SciQLopPlots::interfaces::TimeSpan<QCPTimeSpan, SciQLopPlot>;
}

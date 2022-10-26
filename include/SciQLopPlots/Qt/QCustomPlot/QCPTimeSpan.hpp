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
#include "SciQLopPlots/plot.hpp"
#include <QBrush>
#include <qcp.h>

namespace SciQLopPlots::QCPWrappers
{

class QCPTimeSPanBorder : public QObject, interfaces::GraphicObject
{
    Q_OBJECT

    inline double pixelToXCoord(double px) const
    {
        return line->parentPlot()->axisRect()->axis(QCPAxis::atBottom)->pixelToCoord(px);
    }

    inline double coordToXPixel(double x) const
    {
        return line->parentPlot()->axisRect()->axis(QCPAxis::atBottom)->coordToPixel(x);
    }

    QCPItemStraightLine* line;

public:
    inline QCPTimeSPanBorder(PlotWidget* plot)
            : interfaces::GraphicObject { plot, enums::Layers::Cursors }
            , line { new QCPItemStraightLine { plot->handle() } }
    {
        line->point1->setTypeX(QCPItemPosition::ptAbsolute);
        line->point2->setTypeY(QCPItemPosition::ptAbsolute);
        line->point1->setTypeX(QCPItemPosition::ptAbsolute);
        line->point2->setTypeY(QCPItemPosition::ptAbsolute);
        line->setPen(QPen { QBrush { QColor(0, 255, 255, 255), Qt::SolidPattern }, 3 });
        line->setLayer("overlay");
    }

    inline ~QCPTimeSPanBorder()
    {
        auto plot = line->parentPlot();
        plot->removeItem(line);
        plot->replot(QCustomPlot::rpQueuedReplot);
    }

    inline virtual bool deletable() const override { return false; }

    inline void set_anchor(QCPItemAnchor* top_anchor, QCPItemAnchor* botom_anchor)
    {
        line->point1->setParentAnchor(top_anchor);
        line->point2->setParentAnchor(botom_anchor);
    }

    inline virtual view::data_coordinates<2> center() const override
    {

        return { line->point1->key(), (line->point1->value() + line->point2->value()) / 2 };
    }

    inline virtual view::pixel_coordinates<2> pix_center() const override
    {
        return { line->point1->pixelPosition().x(),
            (line->point1->pixelPosition().y() + line->point2->pixelPosition().y()) / 2 };
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
        return x >= (line->point1->pixelPosition().x() - fmax(line->pen().widthF(), 5))
            && x <= (line->point1->pixelPosition().x() + fmax(line->pen().widthF(), 5));
    }

    inline virtual void set_selected(bool select) override
    {
        line->setSelected(select);
        line->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }

    inline virtual Qt::CursorShape cursor_shape() const override { return Qt::SizeHorCursor; }

    inline virtual void start_edit(const view::pixel_coordinates<2>& position) override { }
    inline virtual void update_edit(const view::pixel_coordinates<2>& position) override { }
    inline virtual void stop_edit(const view::pixel_coordinates<2>& position) override { }

    Q_SIGNAL void move_sig(double dx);
};

class QCPTimeSpan : public QObject
{
    Q_OBJECT
    inline void set_auto_extend_vertically()
    {
        rect->topLeft->setTypeX(QCPItemPosition::ptPlotCoords);
        rect->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
        rect->bottomRight->setTypeX(QCPItemPosition::ptPlotCoords);
        rect->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    }

    inline double pixelToXCoord(double px) const
    {
        return rect->parentPlot()->axisRect()->axis(QCPAxis::atBottom)->pixelToCoord(px);
    }

    QCPItemRect* rect;
    QCPTimeSPanBorder left_border, right_border;

    double mouse_event_initial_left_pos, mouse_event_initial_right_pos;

public:
    using plot_t = QCustomPlotWrapper;

    Q_SIGNAL void range_changed(axis::range new_time_range);

    QCPTimeSpan(PlotWidget* plot, axis::range time_range)
            : rect { new QCPItemRect { plot->handle() } }
            , left_border { plot }
            , right_border { plot }
    {
        left_border.set_anchor(rect->topLeft, rect->bottomLeft);
        right_border.set_anchor(rect->topRight, rect->bottomRight);
        set_auto_extend_vertically();
        set_range(time_range);
        rect->setLayer("overlay");
        rect->setBrush(QBrush { QColor(0, 255, 0, 40), Qt::SolidPattern });
        rect->setPen(QPen { Qt::NoPen });
        rect->setSelectable(true);

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

    ~QCPTimeSpan()
    {
        auto plot = rect->parentPlot();
        if (plot->removeItem(rect))
        {
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }

    inline void set_range(const axis::range& time_range)
    {
        rect->topLeft->setCoords(time_range.first, 0);
        rect->bottomRight->setCoords(time_range.second, 1);
        rect->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        emit range_changed(time_range);
    }

    inline axis::range range() const
    {
        auto t1 = rect->topLeft->key(), t2 = rect->bottomRight->key();
        return { fmin(t1, t2), fmax(t1, t2) };
    };

    inline view::data_coordinates<2> center() const
    {
        return { (rect->topLeft->key() + rect->bottomRight->key()) / 2.,
            (rect->topLeft->value() + rect->bottomRight->value()) / 2. };
    }

    inline view::pixel_coordinates<2> pix_center() const
    {
        return { (rect->topLeft->pixelPosition().x() + rect->bottomRight->pixelPosition().x()) / 2.,
            (rect->topLeft->pixelPosition().y() + rect->bottomRight->pixelPosition().y()) / 2. };
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

        return (x <= rect->bottomRight->key()) && (x >= rect->topLeft->key())
            && (y <= rect->topLeft->value()) && (y >= rect->bottomRight->value());
    }

    inline bool contains(const view::pixel_coordinates<2>& position) const
    {
        const auto x = position.component(enums::Axis::x).value;
        const auto y = position.component(enums::Axis::y).value;

        const auto min_x = rect->topLeft->pixelPosition().x();
        const auto max_x = rect->bottomRight->pixelPosition().x();

        const auto min_y = rect->topLeft->pixelPosition().y();
        const auto max_y = rect->bottomRight->pixelPosition().y();

        return (x <= max_x) && (x >= min_x) && (y <= max_y) && (y >= min_y);
    }

    inline void set_selected(bool select)
    {
        rect->setSelected(select);
        rect->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }

    inline Qt::CursorShape cursor_shape() const { return Qt::SizeAllCursor; }

    inline void start_edit(const view::pixel_coordinates<2>& position)
    {
        auto x = pixelToXCoord(position.component(enums::Axis::x).value);
        set_range({ x, x });
    }
    inline void update_edit(const view::pixel_coordinates<2>& position)
    {
        auto x = pixelToXCoord(position.component(enums::Axis::x).value);
        set_range({ range().first, x });
    }
    inline void stop_edit(const view::pixel_coordinates<2>& position)
    {
        auto x = pixelToXCoord(position.component(enums::Axis::x).value);
        set_range({ range().first, x });
    }
};
}

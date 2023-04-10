/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlotItem.hpp"
#include "constants.hpp"
#include <QBrush>
#include <iostream>
#include <qcustomplot.h>

class VerticalSpanBorder : public QCPItemStraightLine, public SciQLopPlotItem
{
    Q_OBJECT

public:
    Q_SIGNAL void moved(double new_position);
    inline VerticalSpanBorder(QCustomPlot* plot, double x)
            : QCPItemStraightLine { plot }, SciQLopPlotItem {}
    {

        this->point1->setTypeX(QCPItemPosition::ptPlotCoords);
        this->point1->setTypeY(QCPItemPosition::ptAxisRectRatio);
        this->point2->setTypeX(QCPItemPosition::ptPlotCoords);
        this->point2->setTypeY(QCPItemPosition::ptAxisRectRatio);
        this->setPen(QPen { QBrush { QColor(0, 255, 255, 255), Qt::SolidPattern }, 3 });
        this->setSelectable(true);
        this->setMovable(true);
        this->setLayer(Constants::LayersNames::SpansBorders);
        this->set_position(x);
        this->replot();
    }

    double position()
    {
        if (this->point1->coords().x() == this->point2->coords().x())
            return this->point1->coords().x();
        else
            return std::nan("");
    }

    inline void set_position(double x)
    {
        if (this->position() != x)
        {
            this->point1->setCoords(x, 0);
            this->point2->setCoords(x, 1);
            this->replot();
        }
    }

    inline void move(double dx, double dy) override
    {
        this->point1->setPixelPosition(
            { this->point1->pixelPosition().x() + dx, this->point1->pixelPosition().y() });

        this->point2->setPixelPosition(
            { this->point2->pixelPosition().x() + dx, this->point2->pixelPosition().y() });

        emit moved(this->point1->coords().x());
    }
    inline QCPAbstractItem* item() override { return this; }
};


class VerticalSpan : public QCPItemRect, public SciQLopPlotItem
{
    Q_OBJECT
    inline void set_auto_extend_vertically()
    {
        this->topLeft->setTypeX(QCPItemPosition::ptPlotCoords);
        this->bottomRight->setTypeX(QCPItemPosition::ptPlotCoords);

        this->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
        this->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    }

    VerticalSpanBorder* _left_border;
    VerticalSpanBorder* _right_border;

public:
    Q_SIGNAL void range_changed(QCPRange new_time_range);

    VerticalSpan(QCustomPlot* plot, QCPRange horizontal_range)
            : QCPItemRect { plot }
            , SciQLopPlotItem {}
            , _left_border { new VerticalSpanBorder { plot, horizontal_range.lower } }
            , _right_border { new VerticalSpanBorder { plot, horizontal_range.upper } }
    {
        this->setLayer(Constants::LayersNames::Spans);
        this->setBrush(QBrush { QColor(0, 255, 0, 40), Qt::SolidPattern });
        this->setSelectedBrush(QBrush { QColor(255, 0, 255, 40), Qt::SolidPattern });
        this->setPen(QPen { Qt::NoPen });
        this->set_auto_extend_vertically();
        this->setSelectable(true);
        this->setMovable(true);
        this->setVisible(true);
        connect(this->_left_border, &VerticalSpanBorder::moved, this,
            [this](double x)
            {
                this->topLeft->setCoords({ x, 0. });
                this->replot();
                emit this->range_changed(this->range());
            });

        connect(this->_right_border, &VerticalSpanBorder::moved, this,
            [this](double x)
            {
                this->bottomRight->setCoords({ x, 1. });
                this->replot();
                emit this->range_changed(this->range());
            });

        connect(this, &VerticalSpan::selectionChanged, this,
            [](bool s) { std::cout << "selectionChanged " << s << std::endl; });

        this->set_range(horizontal_range);
    }

    ~VerticalSpan()
    {
        this->_left_border->parentPlot()->removeItem(this->_left_border);
        this->_right_border->parentPlot()->removeItem(this->_right_border);
    }

    inline QCPAbstractItem* item() override { return this; }

    inline void set_range(const QCPRange horizontal_range)
    {
        if (this->range() != horizontal_range)
        {
            this->topLeft->setCoords(std::min(horizontal_range.lower, horizontal_range.upper), 10.);
            this->bottomRight->setCoords(
                std::max(horizontal_range.lower, horizontal_range.upper), -10.);
            this->replot();
            emit range_changed(horizontal_range);
        }
    }

    inline QCPRange range() const noexcept
    {
        return QCPRange { this->topLeft->key(), this->bottomRight->key() };
    }

    inline void move(double dx, double dy) override
    {
        this->topLeft->setPixelPosition(
            { this->topLeft->pixelPosition().x() + dx, this->topLeft->pixelPosition().y() });

        this->bottomRight->setPixelPosition({ this->bottomRight->pixelPosition().x() + dx,
            this->bottomRight->pixelPosition().y() });

        this->_left_border->set_position(this->range().lower);
        this->_right_border->set_position(this->range().upper);
        emit this->range_changed(this->range());
    }

    inline double selectTest(
        const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override
    {
        auto left = std::min(this->left->pixelPosition().x(), this->right->pixelPosition().x());
        auto right = std::max(this->left->pixelPosition().x(), this->right->pixelPosition().x());
        auto width = right - left;
        {
            auto left_border_distance
                = this->_left_border->selectTest(pos, onlySelectable, details);
            if (left_border_distance != -1. and left_border_distance <= std::max(10., width * 0.1))
                return -1.;
        }
        {
            auto right_border_distance
                = this->_right_border->selectTest(pos, onlySelectable, details);
            if (right_border_distance != -1.
                and right_border_distance <= std::max(10., width * 0.1))
                return -1.;
        }

        if (pos.x() <= right and pos.x() >= left)
        {
            return 0;
        }
        else
        {

            return std::min(abs(pos.x() - left), abs(pos.x() - right));
        }
    }
};


class SciQLopVerticalSpan : public QObject
{
    Q_OBJECT
    VerticalSpan* _impl;

public:
    Q_SIGNAL void range_changed(QCPRange new_time_range);
    SciQLopVerticalSpan(QCustomPlot* plot, QCPRange horizontal_range)
            : _impl { new VerticalSpan { plot, horizontal_range } }
    {
        connect(
            this->_impl, &VerticalSpan::range_changed, this, &SciQLopVerticalSpan::range_changed);
    }
    ~SciQLopVerticalSpan()
    {
        auto plot = this->_impl->parentPlot();
        if (plot->removeItem(this->_impl))
        {
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }

    inline void set_range(const QCPRange horizontal_range)
    {
        this->_impl->set_range(horizontal_range);
    }
};

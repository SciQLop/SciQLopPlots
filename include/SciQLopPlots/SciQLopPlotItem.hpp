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
#include <qcustomplot.h>


class SciQLopPlotItem
{
protected:
    bool _movable = false;
    QPointF _last_position;

public:
    SciQLopPlotItem() { }
    virtual ~SciQLopPlotItem() { }
    inline bool movable() const noexcept { return this->_movable; }
    inline void setMovable(bool movable) noexcept { this->_movable = movable; }

    inline void setSelected(bool selected) { this->item()->setSelected(selected); }

    virtual void move(double dx, double dy) = 0;
    virtual QCPAbstractItem* item() = 0;
    inline void replot() { this->item()->layer()->replot(); }

    inline void handleMousePressEvent(QMouseEvent* event, const QVariant& details)
    {
        this->_last_position = event->pos();
        event->accept();
    }
    inline void handleMouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
    {
        if(item()->selected() and _movable and event->buttons() == Qt::LeftButton)
        {
            move(event->position().x() - this->_last_position.x(),
                event->position().y() - this->_last_position.y());
        }
        this->_last_position = event->position();
        event->accept();
    }
    inline void handleMouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) { }
};

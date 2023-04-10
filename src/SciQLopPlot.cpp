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
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotItem.hpp"
#include <type_traits>

void SciQLopPlot::mousePressEvent(QMouseEvent* event)
{
    if (this->_selected_item)
    {
        //this->_selected_item->setSelected(false);
        this->_selected_item = nullptr;
    }
    auto maybe_item = this->itemAt(event->position(), true);
    this->_last_position = event->position();
    if (maybe_item != nullptr)
    {
        this->_selected_item = dynamic_cast<SciQLopPlotItem*>(maybe_item);
        if (this->_selected_item)
        {
            //this->_selected_item->setSelected(true);
            //event->accept();
            //return;
        }
    }
    QCustomPlot::mousePressEvent(event);
}

void SciQLopPlot::mouseMoveEvent(QMouseEvent* event)
{
    if (_selected_item and _selected_item->movable() and event->buttons() == Qt::LeftButton)
    {
        _selected_item->move(event->position().x() - this->_last_position->x(),
            event->position().y() - this->_last_position->y());
        _selected_item->item()->layer()->replot();
        this->_last_position = event->position();
        event->accept();
    }
    else
    {
        QCustomPlot::mouseMoveEvent(event);
    }
}

void SciQLopPlot::mouseReleaseEvent(QMouseEvent* event)
{
    //this->_selected_item = nullptr;
    this->_last_position = std::nullopt;
    QCustomPlot::mouseReleaseEvent(event);
}

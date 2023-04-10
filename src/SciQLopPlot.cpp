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
#include <cpp_utils/containers/algorithms.hpp>
#include <type_traits>

void SciQLopPlot::mousePressEvent(QMouseEvent* event)
{
    this->_moved_item = nullptr;
    if (event->buttons() == Qt::LeftButton)
    {
        for (auto item : this->selectedItems())
        {
            if (auto selected_item = dynamic_cast<SciQLopPlotItem*>(item); selected_item
                and selected_item->movable()
                and this->itemAt(event->pos()) == selected_item->item())
            {
                this->_moved_item = selected_item;
                break;
            }
        }
    }
    this->_last_position = event->position();
    QCustomPlot::mousePressEvent(event);
}

void SciQLopPlot::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() == Qt::LeftButton and this->_moved_item)
    {
        this->_moved_item->move(event->position().x() - this->_last_position.x(),
            event->position().y() - this->_last_position.y());
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
    this->_moved_item = nullptr;
    QCustomPlot::mouseReleaseEvent(event);
}

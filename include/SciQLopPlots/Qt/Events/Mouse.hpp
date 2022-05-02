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
#include <QMouseEvent>
#include <optional>

#include "SciQLopPlots/view.hpp"
#include "SciQLopPlots/Interfaces/GraphicObjects/GraphicObject.hpp"

namespace SciQLopPlots::details
{
template <typename plot_t>
inline bool handleMouseMoveEvent(
    const QMouseEvent* event, plot_t* plot, std::optional<QPoint>& last_pressed_pos, interfaces::GraphicObject* selected_graphic_object)
{
    if (last_pressed_pos)
    {
        auto dx = event->pos().x() - last_pressed_pos->x();
        auto dy = event->pos().y() - last_pressed_pos->y();
        if(selected_graphic_object)
        {
            view::move(selected_graphic_object, view::pixel_coordinates { dx, dy });
        }
        else
        {
            view::move(plot, view::pixel_coordinates { dx, dy });
        }
        last_pressed_pos = event->pos();
        return true;
    }
    else
    {
        if(auto go=plot->graphicObjectAt(event->pos()); go)
        {
            plot->setCursor(go->cursor_shape());
        }
        else
        {
            plot->setCursor(Qt::ArrowCursor);
        }
    }
    return false;
}
}

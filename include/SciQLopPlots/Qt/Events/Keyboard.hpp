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

#include "SciQLopPlots/enums.hpp"
#include "SciQLopPlots/view.hpp"
#include "SciQLopPlots/Interfaces/GraphicObjects/GraphicObject.hpp"
#include <QKeyEvent>

namespace SciQLopPlots::details
{
template <typename plot_t>
inline bool handleKeyboardEvent(const QKeyEvent* event, plot_t* plot)
{
    if (event->modifiers() == Qt::NoModifier)
    {
        switch (event->key())
        {
            case Qt::Key_M:
                plot->autoScaleY();
                return true;
            case Qt::Key_Right:
                view::move(plot, 0.2, enums::Axis::x);
                return true;
            case Qt::Key_Left:
                view::move(plot, -0.2, enums::Axis::x);
                return true;
            case Qt::Key_Up:
                view::move(plot, 0.2, enums::Axis::y);
                return true;
            case Qt::Key_Down:
                view::move(plot, -0.2, enums::Axis::y);
                return true;
            default:
                break;
        }
    }
    else if (event->modifiers() == Qt::ControlModifier)
    {
        switch (event->key())
        {
            case Qt::Key_Right:
                view::zoom(plot, 1.2, enums::Axis::x);
                return true;
            case Qt::Key_Left:
                view::zoom(plot, 0.8, enums::Axis::x);
                return true;
            case Qt::Key_Up:
                view::zoom(plot, 1.2, enums::Axis::y);
                return true;
            case Qt::Key_Down:
                view::zoom(plot, 0.8, enums::Axis::y);
                return true;
            default:
                break;
        }
    }
    switch (event->key())
    {
        case Qt::Key_Delete:
            plot->delete_selected_object();
            return true;
        default:
            break;
    }
    return false;
}
}

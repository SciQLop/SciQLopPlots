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
                plot->move(0.2, Qt::Horizontal);
                return true;
            case Qt::Key_Left:
                plot->move(-0.2, Qt::Horizontal);
                return true;
            case Qt::Key_Up:
                plot->move(0.2, Qt::Vertical);
                return true;
            case Qt::Key_Down:
                plot->move(-0.2, Qt::Vertical);
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
                plot->zoom(1.2, Qt::Horizontal);
                return true;
            case Qt::Key_Left:
                plot->zoom(0.8, Qt::Horizontal);
                return true;
            case Qt::Key_Up:
                plot->zoom(1.2, Qt::Vertical);
                return true;
            case Qt::Key_Down:
                plot->zoom(0.8, Qt::Vertical);
                return true;
            default:
                break;
        }
    }
    return false;
}
}

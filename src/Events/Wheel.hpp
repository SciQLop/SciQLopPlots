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
#include <QWheelEvent>
#include <cmath>

template <typename plot_t>
inline void handleWheelEvent(const QWheelEvent* event, plot_t* plot)
{
    constexpr auto factor = 0.85; // stolen from QCP
    double wheelSteps = event->delta() / 120.0; // a single step delta is +/-120 usually
    if (event->modifiers() == Qt::ControlModifier)
    {
        if (event->orientation() == Qt::Vertical)
            plot->zoom(pow(factor, wheelSteps), event->pos().y(), Qt::Vertical);
    }
    else if (event->modifiers() == Qt::AltModifier)
    {
            plot->move(wheelSteps / 10. * pow(0.85, abs(wheelSteps)), Qt::Vertical);
    }
    else if (event->modifiers() == Qt::ShiftModifier)
    {
        if (event->orientation() == Qt::Vertical)
            plot->zoom(pow(factor, wheelSteps), event->pos().x(), Qt::Horizontal);
    }
    else
    {
        if (event->orientation() == Qt::Vertical)
            plot->move(wheelSteps / 10. * pow(0.85, abs(wheelSteps)), Qt::Horizontal);
        else
            plot->move(-wheelSteps / 10. * pow(0.85, abs(wheelSteps)), Qt::Horizontal);
    }
}

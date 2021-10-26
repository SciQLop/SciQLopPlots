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

#include "SciQLopPlots/view.hpp"

namespace SciQLopPlots::details
{
template <typename plot_t>
inline bool handleWheelEvent(const QWheelEvent* event, plot_t* plot)
{
    constexpr auto factor = 0.85; // stolen from QCP
    double wheelSteps = event->angleDelta().y() / 120; // a single step delta is +/-120 usually
    if (event->modifiers() == Qt::ControlModifier)
    {
        if (event->orientation() == Qt::Vertical)
        {
            auto center = plot->mapFromParent(event->pos());
            view::zoom(plot, view::pixel_coordinates { 0, center.y() },
                view::pixel_coordinates { 0, event->angleDelta().y()/8 });
            // plot->zoom(pow(factor, wheelSteps), event->pos(), Qt::Vertical);
        }
    }
    else if (event->modifiers() == Qt::AltModifier)
    {
        // view::move(plot,wheelSteps / 10. * pow(0.85, abs(wheelSteps)), enums::Axis::y);
        view::move(plot, view::pixel_coordinates { 0, event->pixelDelta().y() });
        // plot->move(wheelSteps / 10. * pow(0.85, abs(wheelSteps)), Qt::Vertical);
    }
    else if (event->modifiers() == Qt::ShiftModifier)
    {
        auto center = plot->mapFromParent(event->pos());
        if (event->orientation() == Qt::Vertical)
            view::zoom(plot, view::pixel_coordinates { center.x(), 0 },
                view::pixel_coordinates { event->pixelDelta().x()/100, 0 });
        // plot->zoom(pow(factor, wheelSteps), event->pos(), Qt::Horizontal);
    }
    else
    {
        view::move(
            plot, view::pixel_coordinates { event->pixelDelta().x(), event->pixelDelta().y() });
    }
    return true;
}
}

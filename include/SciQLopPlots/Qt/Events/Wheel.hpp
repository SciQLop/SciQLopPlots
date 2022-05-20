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
#include <QWheelEvent>
#include <cmath>

#include "SciQLopPlots/view.hpp"

namespace SciQLopPlots::details
{

inline double zoomFactor(double wheelAngle)
{
    return 1. + (wheelAngle / 720);
}

inline QPoint wheelMovement(const QWheelEvent* event)
{
    if (event->pixelDelta().x() != 0. or event->pixelDelta().y() != 0.)
    {
        return { event->pixelDelta().x(), event->pixelDelta().y() };
    }
    return { event->angleDelta().x() / 8, event->angleDelta().y() / 8 };
}


template <typename plot_t>
inline bool handleWheelEvent(const QWheelEvent* event, plot_t* plot)
{

    const auto deltapixels = event->pixelDelta();
    const auto deltadegrees = event->angleDelta();

    if (event->modifiers() == Qt::ControlModifier)
    {
        if (event->angleDelta().x() == 0)
        {
            auto center = event->position().toPoint();
            view::zoom(plot, view::single_pixel_coordinate { center.y(), enums::Axis::y },
                zoomFactor(deltadegrees.y()));
        }
    }
    else if (event->modifiers() == Qt::AltModifier)
    {
        view::move(plot, view::pixel_coordinates { 0, deltadegrees.y() });
    }
    else if (event->modifiers() == Qt::ShiftModifier)
    {
        auto center = event->position().toPoint();
        view::zoom(plot, view::single_pixel_coordinate { center.x(), enums::Axis::x },
            zoomFactor(deltadegrees.y()));
    }
    else
    {
        if (!deltapixels.isNull())
            view::move(plot, view::pixel_coordinates { deltapixels.x(), deltapixels.y() });
        else
            view::move(plot, view::pixel_coordinates { deltadegrees.y(), 0. });
    }

    return true;
}
}

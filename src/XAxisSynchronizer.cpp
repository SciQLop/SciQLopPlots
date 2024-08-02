/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/MultiPlots/XAxisSynchronizer.hpp"

void XAxisSynchronizer::_display_x_axis_only_last_plot()
{
    for (auto* plot : _plots)
    {
        plot->x_axis()->set_visible(false);
    }
    if (!_plots.isEmpty())
    {
        _plots.last()->x_axis()->set_visible(true);
    }
}

void XAxisSynchronizer::updatePlotList(const QList<SciQLopPlotInterface*>& plots)
{
    for (auto plot : plots)
    {
        if (!_plots.contains(plot))
        {
            connect(plot, &SciQLopPlotInterface::x_axis_range_changed, this,
                &XAxisSynchronizer::set_x_axis_range);
        }
    }
    for (auto plot : _plots)
    {
        if (!plots.contains(plot))
        {
            disconnect(plot, &SciQLopPlotInterface::x_axis_range_changed, this,
                &XAxisSynchronizer::set_x_axis_range);
        }
    }
    _plots = plots;
    _display_x_axis_only_last_plot();
}

void XAxisSynchronizer::set_x_axis_range(double min, double max)
{
    if (_last_x_range != std::pair { min, max })
    {
        _last_x_range = { min, max };
        for (auto plot : _plots)
        {
            plot->x_axis()->set_range(min, max);
        }
    }
}

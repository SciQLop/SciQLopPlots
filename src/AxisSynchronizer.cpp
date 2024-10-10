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

#include "SciQLopPlots/MultiPlots/AxisSynchronizer.hpp"

void AxisSynchronizer::updatePlotList(const QList<QPointer<SciQLopPlotInterface>>& plots)
{
    for (auto& plot : plots)
    {
        if (!plot.isNull())
        {
            if (!_plots.contains(plot))
            {
                switch (m_sync_axis)
                {
                    case AxisType::XAxis:
                        connect(plot, &SciQLopPlotInterface::x_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    case AxisType::YAxis:
                        connect(plot, &SciQLopPlotInterface::y_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    case AxisType::ZAxis:
                        connect(plot, &SciQLopPlotInterface::z_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    case AxisType::TimeAxis:
                        connect(plot, &SciQLopPlotInterface::time_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    default:
                        break;
                }
            }
        }
    }
    for (auto& plot : _plots)
    {
        if (!plot.isNull())
        {
            if (!plots.contains(plot))
            {
                switch (m_sync_axis)
                {
                    case AxisType::XAxis:
                        disconnect(plot, &SciQLopPlotInterface::x_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    case AxisType::YAxis:
                        disconnect(plot, &SciQLopPlotInterface::y_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    case AxisType::ZAxis:
                        disconnect(plot, &SciQLopPlotInterface::z_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    case AxisType::TimeAxis:
                        disconnect(plot, &SciQLopPlotInterface::time_axis_range_changed, this,
                            &AxisSynchronizer::set_axis_range);
                        break;
                    default:
                        break;
                }
            }
        }
    }
    _plots = plots;
}

void AxisSynchronizer::set_axis_range(const SciQLopPlotRange& range)
{
    for (auto plot : _plots)
    {
        if (!plot.isNull())
        {
            switch (m_sync_axis)
            {
                case AxisType::XAxis:
                    if (auto axis = plot->x_axis())
                        axis->set_range(range);
                    break;
                case AxisType::YAxis:
                    if (auto axis = plot->y_axis())
                        axis->set_range(range);
                    break;
                case AxisType::ZAxis:
                    if (auto axis = plot->z_axis())
                        axis->set_range(range);
                    break;
                case AxisType::TimeAxis:
                    if (auto axis = plot->time_axis())
                        axis->set_range(range);
                    break;
                default:
                    break;
            }
        }
    }
}

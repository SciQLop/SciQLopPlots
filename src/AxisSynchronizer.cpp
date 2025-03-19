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
#include "SciQLopPlots/MultiPlots/SciQLopPlotPanelInterface.hpp"

void _set_axis_range(const SciQLopPlotRange& range, const QPointer<SciQLopPlotInterface>& plot,
                     AxisType m_sync_axis)
{
    if (auto axis = plot->axis(m_sync_axis))
        axis->set_range(range);
}

void AxisSynchronizer::updatePlotList(const QList<QPointer<SciQLopPlotInterface>>& plots)
{
    SciQLopPlotCollectionBehavior::_update_collection(
        _plots, plots,
        [this](SciQLopPlotInterface* plot)
        {
            if (auto axis = plot->axis(this->m_sync_axis))
                connect(axis, &SciQLopPlotAxisInterface::range_changed, this,
                        &AxisSynchronizer::set_axis_range);
        },
        [this](SciQLopPlotInterface* plot)
        {
            if (auto axis = plot->axis(this->m_sync_axis))
                disconnect(axis, &SciQLopPlotAxisInterface::range_changed, this,
                           &AxisSynchronizer::set_axis_range);
        });
}


void AxisSynchronizer::plotAdded(SciQLopPlotInterface* plot)
{
    _set_axis_range(_last_range, plot, m_sync_axis);
}

void AxisSynchronizer::panelAdded(SciQLopPlotPanelInterface* panel)
{
    if (this->m_sync_axis == AxisType::TimeAxis)
    {
        panel->set_time_axis_range(_last_range);
        connect(panel, &SciQLopPlotPanelInterface::time_range_changed, this,
                &AxisSynchronizer::set_axis_range);
    }
}

void AxisSynchronizer::panelRemoved(SciQLopPlotPanelInterface* panel)
{
    if (this->m_sync_axis == AxisType::TimeAxis)
        disconnect(panel, &SciQLopPlotPanelInterface::time_range_changed, this,
                   &AxisSynchronizer::set_axis_range);
}

void AxisSynchronizer::set_axis_range(const SciQLopPlotRange& range)
{
    if (range == _last_range || range.is_valid() == false)
        return;
    this->_last_range = range;
    for (auto plot : _plots)
    {
        if (!plot.isNull())
        {
            _set_axis_range(range, plot, m_sync_axis);
        }
    }
    emit range_changed(range);
}

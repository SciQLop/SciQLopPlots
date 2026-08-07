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
#include "SciQLopPlots/MultiPlots/CrosshairSynchronizer.hpp"
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <cmath>

bool CrosshairSynchronizer::has_sync_axis(SciQLopPlotInterface* plot) const
{
    auto* axis = plot->axis(m_sync_axis);
    return axis && !qobject_cast<SciQLopPlotDummyAxis*>(axis);
}

void CrosshairSynchronizer::updatePlotList(
    const QList<QPointer<SciQLopPlotInterface>>& plots)
{
    _update_collection(
        _plots, plots,
        [this](auto& p) { connect_plot(p); },
        [this](auto& p) { disconnect_plot(p); });
}

void CrosshairSynchronizer::plotAdded(SciQLopPlotInterface* plot)
{
    if (plot && !_plots.contains(plot))
    {
        _plots.append(plot);
        connect_plot(plot);
    }
}

void CrosshairSynchronizer::plotRemoved(SciQLopPlotInterface* plot)
{
    disconnect_plot(plot);
    _plots.removeAll(plot);
}

void CrosshairSynchronizer::connect_plot(SciQLopPlotInterface* plot)
{
    if (plot)
        connect(plot, &SciQLopPlotInterface::cursor_time_changed, this,
                &CrosshairSynchronizer::on_cursor_moved);
}

void CrosshairSynchronizer::disconnect_plot(SciQLopPlotInterface* plot)
{
    if (plot)
        disconnect(plot, nullptr, this, nullptr);
}

void CrosshairSynchronizer::drive_plot(SciQLopPlotInterface* plot, double key) const
{
    // A projection plot's time axis is a placeholder, so has_sync_axis() rejects
    // it and it can never carry a time crosshair. It follows the shared cursor
    // through its trajectory marker instead — this is the time series <-> XY
    // link. set_time_marker clears the markers on a NaN key.
    if (auto* projection = qobject_cast<SciQLopNDProjectionPlot*>(plot))
    {
        if (m_sync_axis == AxisType::TimeAxis)
            projection->set_time_marker(key);
        return;
    }

    if (auto* target = qobject_cast<SciQLopPlot*>(plot))
    {
        if (std::isnan(key))
            target->hide_crosshair();
        else if (has_sync_axis(target))
            target->show_crosshair_at_key(key);
    }
}

void CrosshairSynchronizer::on_cursor_moved(double key)
{
    // show_crosshair_at_key/set_time_marker are deliberately silent, so this is
    // only a guard against a future emitting path re-entering us.
    if (m_propagating)
        return;

    auto* source = qobject_cast<SciQLopPlotInterface*>(sender());
    // A leaving cursor (NaN) always clears every plot; a position is only worth
    // sharing when the source itself sits on the synchronized axis.
    if (!std::isnan(key) && !(source && has_sync_axis(source)))
        return;

    m_propagating = true;
    for (const auto& plot : _plots)
    {
        if (!plot.isNull() && plot.data() != source)
            drive_plot(plot.data(), key);
    }
    m_propagating = false;
}

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
    if (auto* p = qobject_cast<SciQLopPlot*>(plot))
    {
        auto* impl = static_cast<_impl::SciQLopPlot*>(p->qcp_plot());
        connect(impl, &_impl::SciQLopPlot::hover_x_changed,
                this, &CrosshairSynchronizer::on_hover_x_changed);
    }
}

void CrosshairSynchronizer::disconnect_plot(SciQLopPlotInterface* plot)
{
    if (auto* p = qobject_cast<SciQLopPlot*>(plot))
        disconnect(p->qcp_plot(), nullptr, this, nullptr);
}

void CrosshairSynchronizer::on_hover_x_changed(double key)
{
    if (m_propagating)
        return;
    m_propagating = true;

    auto* source = sender();

    // Find the source SciQLopPlotInterface to check its axis
    SciQLopPlotInterface* src_plot = nullptr;
    for (auto& plot : _plots)
    {
        if (plot.isNull())
            continue;
        if (auto* p = qobject_cast<SciQLopPlot*>(plot.data()))
        {
            if (p->qcp_plot() == source)
            {
                src_plot = p;
                break;
            }
        }
    }

    bool src_has_axis = src_plot && has_sync_axis(src_plot);

    for (auto& plot : _plots)
    {
        if (plot.isNull())
            continue;
        if (auto* p = qobject_cast<SciQLopPlot*>(plot.data()))
        {
            auto* impl = static_cast<_impl::SciQLopPlot*>(p->qcp_plot());
            if (impl == source)
                continue;
            if (std::isnan(key))
            {
                impl->hide_crosshair();
                continue;
            }
            // Only sync if both source and target participate in axis sync
            if (!src_has_axis || !has_sync_axis(p))
                continue;
            impl->show_crosshair_at_key(key);
        }
    }
    m_propagating = false;
}

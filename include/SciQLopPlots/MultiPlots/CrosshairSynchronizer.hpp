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
#pragma once

#include "SciQLopPlotCollection.hpp"
#include "SciQLopPlots/enums.hpp"

/*!
 * \brief Shares one cursor position across every plot of a collection.
 *
 * Listens to SciQLopPlotInterface::cursor_time_changed and mirrors the key onto
 * the other plots: a SciQLopPlot follows it with its crosshair, a
 * SciQLopNDProjectionPlot — whose time axis is only a placeholder — follows it
 * with its trajectory time marker. That second path is what links a time series
 * to the XY projections of the same data.
 */
class CrosshairSynchronizer : public SciQLopPlotCollectionBehavior
{
    Q_OBJECT
    bool m_propagating = false;
    AxisType m_sync_axis;

public:
    CrosshairSynchronizer(QObject* parent, AxisType axis)
        : SciQLopPlotCollectionBehavior(parent), m_sync_axis(axis) {}

    Q_SLOT void updatePlotList(
        const QList<QPointer<SciQLopPlotInterface>>& plots) override;
    Q_SLOT void plotAdded(SciQLopPlotInterface* plot) override;
    Q_SLOT void plotRemoved(SciQLopPlotInterface* plot) override;

private:
    bool has_sync_axis(SciQLopPlotInterface* plot) const;
    void connect_plot(SciQLopPlotInterface* plot);
    void disconnect_plot(SciQLopPlotInterface* plot);
    void drive_plot(SciQLopPlotInterface* plot, double key) const;
    Q_SLOT void on_cursor_moved(double key);
};

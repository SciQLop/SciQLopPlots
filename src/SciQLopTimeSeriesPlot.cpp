/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/SciQLopTimeSeriesPlot.hpp"

SciQLopTimeSeriesPlot::SciQLopTimeSeriesPlot(QWidget* parent) : SciQLopPlot { parent }
{
    auto date_ticker = QSharedPointer<QCPAxisTickerDateTime>::create();
    date_ticker->setDateTimeFormat("yyyy/MM/dd \nhh:mm:ss.zzz");
    date_ticker->setDateTimeSpec(Qt::UTC);
    qcp_plot()->xAxis->setTicker(date_ticker);
    set_axes_to_rescale(QList<SciQLopPlotAxisInterface*> { y_axis(), y2_axis(), z_axis() });
    freeze_axis(x_axis());
    x_axis()->set_is_time_axis(true);
    connect(this, &SciQLopTimeSeriesPlot::x_axis_range_changed, this,
            &SciQLopTimeSeriesPlot::time_axis_range_changed);
}

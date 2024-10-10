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

#include "SciQLopPlots/SciQLopPlot.hpp"

class SciQLopTimeSeriesPlot : public SciQLopPlot
{
    Q_OBJECT

public:
    explicit SciQLopTimeSeriesPlot(QWidget* parent = nullptr);
    virtual ~SciQLopTimeSeriesPlot() Q_DECL_OVERRIDE = default;

    inline QList<PyBuffer> unity(const QList<PyBuffer>& views) { return views; }

    inline PyBuffer unity(const PyBuffer& view) { return view; }

    inline virtual SciQLopPlotAxisInterface* time_axis() const noexcept override
    {
        return x_axis();
    }

    Q_SLOT inline virtual void set_time_range(const SciQLopPlotRange& range)
    {
        this->x_axis()->set_range(range);
    }

    Q_SLOT inline virtual void set_time_range(double start, double stop)
    {
        this->x_axis()->set_range(SciQLopPlotRange { start, stop });
    }

    Q_SLOT inline virtual void set_time_range(const QDateTime& start, const QDateTime& stop)
    {
        this->x_axis()->set_range(SciQLopPlotRange { start, stop });
    }
};

inline QList<SciQLopTimeSeriesPlot*> only_sciqlop_timeserieplots(
    const QList<SciQLopPlotInterface*>& plots)
{
    QList<SciQLopTimeSeriesPlot*> filtered;
    for (auto plot : plots)
    {
        if (auto p = dynamic_cast<SciQLopTimeSeriesPlot*>(plot))
            filtered.append(p);
    }
    return filtered;
}

inline QList<QPointer<SciQLopTimeSeriesPlot>> only_sciqlop_timeserieplots(
    const QList<QPointer<SciQLopPlotInterface>>& plots)
{
    QList<QPointer<SciQLopTimeSeriesPlot>> filtered;
    for (auto& plot : plots)
    {
        if (!plot.isNull())
        {
            if (auto p = dynamic_cast<SciQLopTimeSeriesPlot*>(plot.data()))
                filtered.append(p);
        }
    }
    return filtered;
}

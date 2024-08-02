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

class XAxisSynchronizer : public SciQLopPlotCollectionBehavior
{
    Q_OBJECT
    QList<SciQLopPlotInterface*> _plots;
    std::pair<double, double> _last_x_range;

    void _display_x_axis_only_last_plot();

public:
    XAxisSynchronizer(QObject* parent = nullptr) : SciQLopPlotCollectionBehavior(parent) { }

    Q_SLOT void updatePlotList(const QList<SciQLopPlotInterface*>& plots) override;

    Q_SLOT void set_x_axis_range(double min, double max);
};

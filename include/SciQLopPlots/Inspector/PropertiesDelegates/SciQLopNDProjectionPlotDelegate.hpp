/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"

//! Inspector panel for a projection plot: how its axes follow each other, square panes, and
//! the options shared by its panes. Each pane has the plot panel of its own.
class SciQLopNDProjectionPlotDelegate : public PropertyDelegateBase
{
    Q_OBJECT

    SciQLopNDProjectionPlot* projection() const
    {
        return as_type<SciQLopNDProjectionPlot>(m_object);
    }

    void add_axis_linking_row(SciQLopNDProjectionPlot* plot);
    void add_square_panes_row(SciQLopNDProjectionPlot* plot);
    void add_switch(const QString& text, bool value, void (SciQLopNDProjectionPlot::*setter)(bool));

public:
    using compatible_type = SciQLopNDProjectionPlot;
    SciQLopNDProjectionPlotDelegate(SciQLopNDProjectionPlot* object, QWidget* parent = nullptr);
    ~SciQLopNDProjectionPlotDelegate() override = default;
};

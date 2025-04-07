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

#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LegendDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotAxisDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertyDelegates.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"

REGISTER_DELEGATE(SciQLopPlotDelegate);

SciQLopPlot* SciQLopPlotDelegate::plot() const
{
    return as_type<SciQLopPlot>(m_object);
}

SciQLopPlotDelegate::SciQLopPlotDelegate(SciQLopPlot* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto legend = object->legend();
    auto legend_delegate = new LegendDelegate(legend->is_visible(), this);
    m_layout->addRow(legend_delegate);
    connect(legend_delegate, &LegendDelegate::visibility_changed, legend,
            &SciQLopPlotLegendInterface::set_visible);
    connect(legend, &SciQLopPlotLegendInterface::visibility_changed, legend_delegate,
            &LegendDelegate::set_visible);
    auto auto_scale = new BooleanDelegate(object->auto_scale());
    m_layout->addRow("Auto scale", auto_scale);
    connect(auto_scale, &BooleanDelegate::value_changed, object, &SciQLopPlot::set_auto_scale);
    connect(object, &SciQLopPlot::auto_scale_changed, auto_scale, &BooleanDelegate::set_value);
}

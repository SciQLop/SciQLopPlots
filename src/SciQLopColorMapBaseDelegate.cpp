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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>

SciQLopColorMapBase* SciQLopColorMapBaseDelegate::color_map_base() const
{
    return as_type<SciQLopColorMapBase>(m_object);
}

SciQLopColorMapBaseDelegate::SciQLopColorMapBaseDelegate(SciQLopColorMapBase* object,
                                                         QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto* visibleCheck = new BooleanDelegate("Visible", object->visible(), this);
    m_layout->addRow(visibleCheck);
    connect(visibleCheck, &BooleanDelegate::value_changed, object,
            &SciQLopPlottableInterface::set_visible);
    connect(object, &SciQLopPlottableInterface::visible_changed, visibleCheck,
            [visibleCheck](bool v)
            {
                QSignalBlocker blocker(visibleCheck);
                visibleCheck->set_value(v);
            });
    // Color-scale controls (gradient, autoscale percentile) live on the
    // "Color Scale" (z) axis node. Product-specific knobs are added by derived
    // delegates (Contours, Binning/normalization).
}

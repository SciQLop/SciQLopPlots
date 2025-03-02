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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphComponentDelegate.hpp"

#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LineDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertyDelegates.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"

REGISTER_DELEGATE(SciQLopGraphComponentDelegate);

SciQLopGraphComponentInterface* SciQLopGraphComponentDelegate::component() const
{
    return as_type<SciQLopGraphComponentInterface>(m_object);
}

SciQLopGraphComponentDelegate::SciQLopGraphComponentDelegate(SciQLopGraphComponentInterface* object,
                                                             QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    m_lineDelegate = new LineDelegate(object->pen(), object->line_style(), object->marker_shape(), this);
    m_layout->addWidget(m_lineDelegate);
    connect(m_lineDelegate, &LineDelegate::penChanged, object,
            &SciQLopGraphComponentInterface::set_pen);
    connect(m_lineDelegate, &LineDelegate::styleChanged, object,
            &SciQLopGraphComponentInterface::set_line_style);
    connect(m_lineDelegate, &LineDelegate::markerShapeChanged, object,
            &SciQLopGraphComponentInterface::set_marker_shape);
}

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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopMultiPlotPanelDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertyDelegates.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include <QPushButton>
#include <QVBoxLayout>

REGISTER_DELEGATE(SciQLopMultiPlotPanelDelegate);

SciQLopMultiPlotPanel* SciQLopMultiPlotPanelDelegate::panel() const
{
    return as_type<SciQLopMultiPlotPanel>(m_object);
}

SciQLopMultiPlotPanelDelegate::SciQLopMultiPlotPanelDelegate(SciQLopMultiPlotPanel* object,
                                                             QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
}

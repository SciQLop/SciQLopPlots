/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"

BooleanDelegate::BooleanDelegate(bool value, QWidget* parent) : BooleanDelegate("", value, parent)
{
}

BooleanDelegate::BooleanDelegate(const QString &text, bool value, QWidget *parent): QCheckBox(text, parent)
{
    setChecked(value);
    connect(this, &QCheckBox::checkStateChanged, this,
            [this](auto state) { emit value_changed(state == Qt::Checked); });
}

void BooleanDelegate::set_value(const bool value)
{
    setChecked(value);
}

bool BooleanDelegate::value() const
{
    return isChecked();
}

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
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/unique_names_factory.hpp"

SciQLopGraphInterface::SciQLopGraphInterface(const QString& prefix, QObject* parent)
        : SciQLopPlottableInterface(parent)
{
    connect(this, &QObject::objectNameChanged, this, &SciQLopGraphInterface::name_changed);
    setObjectName(UniqueNamesFactory::unique_name(prefix));
}

void SciQLopPlottableInterface::set_range(const SciQLopPlotRange& range)
{
    if (m_range != range)
    {
        m_range = range;
        Q_EMIT range_changed(range);
    }
}

SciQLopColorMapInterface::SciQLopColorMapInterface(QObject* parent)
        : SciQLopPlottableInterface(parent)
{
    setObjectName(UniqueNamesFactory::unique_name("ColorMap"));
}

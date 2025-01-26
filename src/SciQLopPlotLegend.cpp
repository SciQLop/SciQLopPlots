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

#include "SciQLopPlots/SciQLopPlotLegend.hpp"
#include "SciQLopPlots/Debug.hpp"
#include <qcp.h>

SciQLopPlotLegend::SciQLopPlotLegend(QCPLegend* legend, QObject* parent)
        : SciQLopPlotLegendInterface(parent), m_legend(legend)
{
    legend->setSelectableParts(QCPLegend::spItems);
}

bool SciQLopPlotLegend::is_visible() const
{
    return m_legend->visible();
}

void SciQLopPlotLegend::set_visible(bool visible)
{
    if (visible != is_visible())
    {
        m_legend->setVisible(visible);
        m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT visibility_changed(visible);
    }
}

QPointF SciQLopPlotLegend::position() const
{
    WARN_UNSUPPORTED_FUNCTIONALITY;
    return QPointF();
}

void SciQLopPlotLegend::set_position(const QPointF&)
{
    WARN_UNSUPPORTED_FUNCTIONALITY;
}

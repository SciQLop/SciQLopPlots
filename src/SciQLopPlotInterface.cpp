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

#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/SciQLopTheme.hpp"
#include <QColor>

void SciQLopPlotInterface::apply_selection_style()
{
    if (m_selected)
    {
        QColor border_color = Qt::black;
        if (auto* t = this->theme(); t)
        {
            const QColor themed = t->selection();
            if (themed.isValid())
                border_color = themed;
        }
        this->setStyleSheet(
            QStringLiteral("border: 2px solid %1;border-style: dashed;").arg(border_color.name()));
    }
    else
    {
        this->setStyleSheet(QStringLiteral("border: 0px;"));
    }
}

void SciQLopPlotInterface::set_selected(bool selected) noexcept
{
    if (m_selected != selected)
    {
        m_selected = selected;
        apply_selection_style();
        Q_EMIT selection_changed(selected);
    }
}

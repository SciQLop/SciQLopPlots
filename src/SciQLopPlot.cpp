/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotItem.hpp"
#include <algorithm>
#include <cpp_utils/containers/algorithms.hpp>
#include <type_traits>

void SciQLopPlot::mousePressEvent(QMouseEvent* event)
{
    QCustomPlot::mousePressEvent(event);
}

void SciQLopPlot::mouseMoveEvent(QMouseEvent* event)
{
    QCustomPlot::mouseMoveEvent(event);
}

void SciQLopPlot::mouseReleaseEvent(QMouseEvent* event)
{
    QCustomPlot::mouseReleaseEvent(event);
}

void SciQLopPlot::keyPressEvent(QKeyEvent* event)
{
    auto items = selectedItems();
    std::for_each(items.begin(), items.end(),
        [event](auto item)
        {
            if (auto sciItem = dynamic_cast<SciQLopItemWithKeyInteraction*>(item);
                sciItem != nullptr)
            {
                sciItem->keyPressEvent(event);
            }
        });
    if (event->isAccepted() == false)
        QCustomPlot::keyPressEvent(event);
}

bool SciQLopPlot::event(QEvent* event)
{

    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        auto itm = dynamic_cast<SciQlopItemWithToolTip*>(itemAt(helpEvent->pos(), true));
        if (itm)
        {
            QToolTip::showText(helpEvent->globalPos(), itm->tooltip());
        }
        else
            QToolTip::hideText();

        return true;
    }

    return QWidget::event(event);
}

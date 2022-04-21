/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2020, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Qt/Events/Keyboard.hpp"
#include "SciQLopPlots/Qt/Events/Mouse.hpp"
#include "SciQLopPlots/Qt/Events/Wheel.hpp"
#include "SciQLopPlots/Interfaces/PlotWidget.hpp"

#include <QVBoxLayout>

namespace SciQLopPlots
{

void IPlotWidget::setWidget(QWidget* widget)
{
    if (!this->layout())
        this->setLayout(new QVBoxLayout);
    this->layout()->addWidget(widget);
}

void IPlotWidget::wheelEvent(QWheelEvent* event)
{
    if (details::handleWheelEvent(event, this))
        event->accept();
}

void IPlotWidget::keyPressEvent(QKeyEvent* event)
{
    if (details::handleKeyboardEvent(event, this))
        event->accept();
}

void IPlotWidget::keyReleaseEvent(QKeyEvent* event)
{
    event->accept();
}

void IPlotWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        this->m_lastMousePress = event->pos();
    event->accept();
}

void IPlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (details::handleMouseMoveEvent(event, this, this->m_lastMousePress))
        event->accept();
}

void IPlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
    this->m_lastMousePress = std::nullopt;
    event->accept();
}


}


#include "IPlotWidget.moc"

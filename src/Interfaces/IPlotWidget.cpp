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
#include "SciQLopPlots/Interfaces/IPlotWidget.hpp"
#include "SciQLopPlots/Interfaces/PlotWidget.hpp"
#include "SciQLopPlots/Qt/Events/Keyboard.hpp"
#include "SciQLopPlots/Qt/Events/Mouse.hpp"
#include "SciQLopPlots/Qt/Events/Wheel.hpp"

#include <QVBoxLayout>

#include <iostream>

namespace SciQLopPlots::interfaces
{

void IPlotWidget::registerGraphicObject(interfaces::GraphicObject* go)
{
    graphic_objects.push_back(go);
}

void IPlotWidget::removeGraphicObject(interfaces::GraphicObject* go)
{
    if (std::size(graphic_objects))
    {
        if (auto it = std::find(std::begin(graphic_objects), std::end(graphic_objects), go);
            it != std::end(graphic_objects))
        {
            std::swap(*it, graphic_objects.back());
            graphic_objects.pop_back();
        }
    }
}


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
    m_selected_object = graphicObjectAt(event->pos());
    event->accept();
}

void IPlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (details::handleMouseMoveEvent(event, this, this->m_lastMousePress, m_selected_object))
        event->accept();
}

void IPlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
    this->m_lastMousePress = std::nullopt;
    event->accept();
}


}


#include "IPlotWidget.moc"

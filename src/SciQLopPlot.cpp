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

void SciQLopPlot::set_scroll_factor(double factor) noexcept
{
    m_scroll_factor = factor;
    Q_EMIT scroll_factor_changed(factor);
}

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

void SciQLopPlot::_wheel_zoom(QCPAxis* axis, const double wheelSteps, const QPointF& pos)
{
    const auto factor = qPow(axis->axisRect()->rangeZoomFactor(axis->orientation()), wheelSteps);
    axis->scaleRange(
        factor, axis->pixelToCoord(axis->orientation() == Qt::Horizontal ? pos.x() : pos.y()));
}

void SciQLopPlot::_wheel_pan(QCPAxis* axis, const double wheelSteps, const QPointF& pos)
{
    axis->moveRange(wheelSteps * m_scroll_factor * QApplication::wheelScrollLines() / 100.
        * axis->range().size());
}

void SciQLopPlot::wheelEvent(QWheelEvent* event)
{
    const auto pos = event->position();
    const auto wheelSteps = event->angleDelta().y() / 120.0;
    foreach (QCPLayerable* candidate, layerableListAt(pos, false))
    {
        if (auto axis = qobject_cast<QCPAxis*>(candidate); axis != nullptr)
        {
            this->_wheel_zoom(axis, wheelSteps, pos);
            event->accept();
            this->replot(rpQueuedReplot);
            return;
        }
        else if (auto axisRect = qobject_cast<QCPAxisRect*>(candidate); axisRect != nullptr)
        {
            if (event->modifiers().testFlags(Qt::ShiftModifier | Qt::ControlModifier))
                this->_wheel_pan(axisRect->axis(QCPAxis::atLeft), wheelSteps, pos);
            else if (event->modifiers().testFlag(Qt::ControlModifier))
                this->_wheel_zoom(axisRect->axis(QCPAxis::atBottom), wheelSteps, pos);
            else if (event->modifiers().testFlag(Qt::ShiftModifier))
                this->_wheel_zoom(axisRect->axis(QCPAxis::atLeft), wheelSteps, pos);
            else
                this->_wheel_pan(axisRect->axis(QCPAxis::atBottom), wheelSteps, pos);
            this->replot(rpQueuedReplot);
            event->accept();
            return;
        }
    }
    QCustomPlot::wheelEvent(event);
}

void SciQLopPlot::keyPressEvent(QKeyEvent* event)
{
    auto items = selectedItems();
    event->setAccepted(false);
    std::for_each(items.begin(), items.end(),
        [event, this](auto item)
        {
            if (auto sciItem = dynamic_cast<SciQLopItemWithKeyInteraction*>(item);
                sciItem != nullptr)
            {
                sciItem->keyPressEvent(event);
            }
            else if (auto axis = dynamic_cast<QCPAxis*>(item); axis != nullptr)
            {
                if (axis->orientation() == Qt::Vertical)
                {
                    switch (event->key())
                    {
                        case Qt::Key_L:
                        {
                            if (axis->scaleType() == QCPAxis::stLinear)
                                axis->setScaleType(QCPAxis::stLogarithmic);
                            else
                                axis->setScaleType(QCPAxis::stLinear);
                            event->accept();
                            this->replot(rpQueuedReplot);
                        }
                        case Qt::Key_M:
                        {
                            axis->rescale(true);
                            event->accept();
                            this->replot(rpQueuedReplot);
                        }
                    }
                }
            }
        });
    if (event->isAccepted() == false)
    {
        switch (event->key())
        {
            case Qt::Key_L:
            {
                if (axisRect()->axis(QCPAxis::atLeft)->scaleType() == QCPAxis::stLinear)
                    axisRect()->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
                else
                    axisRect()->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLinear);
                event->accept();
                this->replot(rpQueuedReplot);
            }
            case Qt::Key_M:
            {
                axisRect()->axis(QCPAxis::atLeft)->rescale(true);
                if (auto a = axisRect()->axis(QCPAxis::atRight); a != nullptr)
                    a->rescale(true);
                event->accept();
                this->replot(rpQueuedReplot);
            }
        }
    }
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

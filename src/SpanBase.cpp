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
#include "SciQLopPlots/Items/impl/SpanBase.hpp"

namespace impl
{

SpanBase::SpanBase(QCPAbstractSpanItem* span) : _span { span } { }

void SpanBase::set_color(const QColor& color)
{
    _span->setBrush(QBrush(color, Qt::SolidPattern));
    _span->setSelectedBrush(QBrush(
        QColor(255 - color.red(), 255 - color.green(), 255 - color.blue(), color.alpha()),
        Qt::SolidPattern));
    _span->setPen(QPen(Qt::NoPen));
    _span->setSelectedPen(QPen(Qt::NoPen));
}

QColor SpanBase::color() const noexcept { return _span->brush().color(); }

void SpanBase::set_borders_color(const QColor& color)
{
    _span->setBorderPen(QPen(QBrush(color, Qt::SolidPattern), 3));
}

QColor SpanBase::borders_color() const noexcept { return _span->borderPen().color(); }

void SpanBase::set_visible(bool visible) { _span->setVisible(visible); }

void SpanBase::set_borders_tool_tip(const QString& tool_tip)
{
    SciQlopItemWithToolTip::setToolTip(tool_tip);
}

void SpanBase::replot(bool immediate)
{
    if (immediate)
    {
        if (auto* l = _span->layer())
            l->replot();
    }
    else
        _span->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

} // namespace impl

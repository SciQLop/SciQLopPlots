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
#include "SciQLopPlots/Items/SciQLopSpanBase.hpp"

SciQLopSpanBase::SciQLopSpanBase(impl::SpanBase* impl_base)
    : _impl_base { impl_base }, _span { impl_base->span() }
{
}

SciQLopSpanBase::~SciQLopSpanBase()
{
    if (_span)
    {
        auto* plot = _span->parentPlot();
        if (plot && plot->hasItem(_span.data()))
        {
            (void)plot->removeItem(_span.data());
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }
}

void SciQLopSpanBase::set_visible(bool visible)
{
    if (_span)
        _impl_base->set_visible(visible);
}

bool SciQLopSpanBase::visible() const
{
    return _span ? _span->visible() : false;
}

void SciQLopSpanBase::set_color(const QColor& color)
{
    if (_span)
        _impl_base->set_color(color);
}

QColor SciQLopSpanBase::color() const
{
    return _span ? _impl_base->color() : QColor {};
}

void SciQLopSpanBase::set_borders_color(const QColor& color)
{
    if (_span)
        _impl_base->set_borders_color(color);
}

QColor SciQLopSpanBase::borders_color() const
{
    return _span ? _impl_base->borders_color() : QColor {};
}

void SciQLopSpanBase::set_selected(bool selected)
{
    if (_span)
        _span->setSelected(selected);
}

bool SciQLopSpanBase::selected() const
{
    return _span ? _span->selected() : false;
}

void SciQLopSpanBase::set_read_only(bool read_only)
{
    if (_span)
        _span->setMovable(!read_only);
}

bool SciQLopSpanBase::read_only() const
{
    return _span ? !_span->movable() : true;
}

void SciQLopSpanBase::set_tool_tip(const QString& tool_tip)
{
    if (_span)
    {
        _impl_base->setToolTip(tool_tip);
        _impl_base->set_borders_tool_tip(tool_tip);
    }
}

QString SciQLopSpanBase::tool_tip() const
{
    return _span ? _impl_base->tooltip() : QString {};
}

void SciQLopSpanBase::replot()
{
    if (_span)
        _impl_base->replot();
}

QCustomPlot* SciQLopSpanBase::parentPlot() const
{
    return _span ? _span->parentPlot() : nullptr;
}

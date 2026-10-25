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
#include "SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp"

void MultiPlotsVerticalSpan::_set_visible(bool visible)
{
    const QPointer<MultiPlotsVerticalSpan> alive(this);
    SciQLopSpan::_set_visible(visible && m_in_range);
    if (alive)
        m_shown = visible;
}

void MultiPlotsVerticalSpan::set_in_range(bool in_range)
{
    SCIQLOP_ON_OWNER_THREAD(set_in_range(in_range));
    m_in_range = in_range;
    const bool effective = _visible();
    const QPointer<MultiPlotsVerticalSpan> alive(this);
    SciQLopSpan::_set_visible(effective);
    if (alive)
        _notify_visible(effective);
}

SciQLopMultiPlotPanel* MultiPlotsVSpanCollection::panel()
{
    return dynamic_cast<SciQLopMultiPlotPanel*>(parent());
}

void MultiPlotsVSpanCollection::updateVisibleSpans(const SciQLopPlotRange& horizontal_range)
{
    // Over a snapshot, like the rest of this file: a span's visibility slot may delete a sibling.
    const auto spans = _spans;
    for (const auto& span : spans)
        if (span)
            span->set_in_range(horizontal_range.intersects(span->range()));
}

void MultiPlotsVSpanCollection::delete_span(const QString &id)
{
    SCIQLOP_ON_OWNER_THREAD(delete_span(id));
    auto vspan = span(id);
    if (vspan)
    {
        delete_span(vspan);
    }
}

QPointer<MultiPlotsVerticalSpan>
MultiPlotsVSpanCollection::create_span(SciQLopPlotRange horizontal_range, QColor color,
                                        bool read_only, const QString tool_tip, const QString id)
{
    SciQLopPlots::require_owner_thread(this, "MultiPlotsVSpanCollection::create_span");
    auto p = panel();
    MultiPlotsVerticalSpan* vspan = new MultiPlotsVerticalSpan(
        p, horizontal_range, color, read_only, true, tool_tip, id);
    vspan->set_in_range(horizontal_range.intersects(p->time_axis_range()));
    _spans.append(vspan);
    connect(vspan, &MultiPlotsVerticalSpan::destroyed, this,
            [this, vspan]() { _spans.removeAll(vspan); });
    return vspan;
}

void MultiPlotsVSpanCollection::delete_span(QPointer<MultiPlotsVerticalSpan> vspan)
{
    SCIQLOP_ON_OWNER_THREAD(delete_span(vspan));
    _spans.removeAll(vspan);
    delete vspan;
}

QList<QPointer<MultiPlotsVerticalSpan>> MultiPlotsVSpanCollection::spans() const
{
    return _spans;
}

QList<QPointer<MultiPlotsVerticalSpan>>
MultiPlotsVSpanCollection::spans_in_range(SciQLopPlotRange horizontal_range) const
{
    QList<QPointer<MultiPlotsVerticalSpan>> result;
    for (const auto& span : _spans)
    {
        if (span && horizontal_range.intersects(span->range()))
        {
            result.append(span);
        }
    }
    return result;
}

QPointer<MultiPlotsVerticalSpan> MultiPlotsVSpanCollection::span(const QString& id) const
{
    for (auto& span : _spans)
    {
        if (span && span->id() == id)
        {
            return span;
        }
    }
    return nullptr;
}

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

void MultiPlotsVerticalSpan::select_lower_border(bool selected)
{
    if (_lower_border_selected != selected)
    {
        _lower_border_selected = selected;
        const QPointer<MultiPlotsVerticalSpan> self(this);
        for_each_span([selected](SciQLopVerticalSpan* span) { span->select_lower_border(selected); });
        if (self)
            replotAll();
    }
}

void MultiPlotsVerticalSpan::select_upper_border(bool selected)
{
    if (_upper_border_selected != selected)
    {
        _upper_border_selected = selected;
        const QPointer<MultiPlotsVerticalSpan> self(this);
        for_each_span([selected](SciQLopVerticalSpan* span) { span->select_upper_border(selected); });
        if (self)
            replotAll();
    }
}

void MultiPlotsVerticalSpan::addObject(SciQLopPlotInterface* plot)
{
    if (auto scp = dynamic_cast<SciQLopPlot*>(plot); scp)
    {
        auto new_span
            = new SciQLopVerticalSpan(scp, _horizontal_range, m_color, _read_only, m_visible, _tool_tip);
        new_span->set_selected(m_selected);
        QObject::connect(new_span, &SciQLopVerticalSpan::range_changed, this,
                         &MultiPlotsVerticalSpan::set_range);
        QObject::connect(new_span, &SciQLopVerticalSpan::selection_changed, this,
                         &MultiPlotsVerticalSpan::set_selected);
        QObject::connect(new_span, &SciQLopVerticalSpan::lower_border_selection_changed, this,
                         &MultiPlotsVerticalSpan::select_lower_border);
        QObject::connect(new_span, &SciQLopVerticalSpan::upper_border_selection_changed, this,
                         &MultiPlotsVerticalSpan::select_upper_border);
        QObject::connect(new_span, &SciQLopVerticalSpan::delete_requested, this,
                         &MultiPlotsVerticalSpan::delete_requested);
        _spans.append(new_span);
    }
}

void MultiPlotsVerticalSpan::removeObject(SciQLopPlotInterface* plot)
{
    // Drop spans whose plot was destroyed directly (null QPointers) — they
    // can never match a removeObject call, so purge them here.
    _spans.removeIf([](const QPointer<SciQLopVerticalSpan>& span) { return span.isNull(); });
    if (auto scp = dynamic_cast<SciQLopPlot*>(plot); scp)
    {
        for (int i = _spans.size() - 1; i >= 0; --i)
        {
            if (_spans[i] && _spans[i]->parentPlot() == scp->qcp_plot())
            {
                delete _spans[i];
                _spans.removeAt(i);
            }
        }
    }
}

void MultiPlotsVerticalSpan::_set_selected(bool selected)
{
    // Stored first: each plot's span echoes the selection back through set_selected.
    m_selected = selected;
    const QPointer<MultiPlotsVerticalSpan> self(this);
    for_each_span([selected](SciQLopVerticalSpan* span) { span->set_selected(selected); });
    if (self)
        replotAll();
}

// Stored after forwarding, unlike selection (no echo here): a plot span that raises leaves the
// panel span reading its old value, so a retry is not taken for a repeat.
void MultiPlotsVerticalSpan::_set_color(const QColor& color)
{
    const QPointer<MultiPlotsVerticalSpan> self(this);
    for_each_span([&color](SciQLopVerticalSpan* span) { span->set_color(color); });
    if (self)
        m_color = color;
}

void MultiPlotsVerticalSpan::_set_visible(bool visible)
{
    const QPointer<MultiPlotsVerticalSpan> self(this);
    for_each_span([visible](SciQLopVerticalSpan* span) { span->set_visible(visible); });
    if (self)
        m_visible = visible;
}

void MultiPlotsVerticalSpan::set_range(const SciQLopPlotRange horizontal_range)
{
    SCIQLOP_ON_OWNER_THREAD(set_range(horizontal_range));
    if (horizontal_range != _horizontal_range)
    {
        _horizontal_range = horizontal_range;
        const QPointer<MultiPlotsVerticalSpan> self(this);
        for_each_span([&horizontal_range](SciQLopVerticalSpan* span)
                      { span->set_range(horizontal_range); });
        if (self)
            Q_EMIT range_changed(horizontal_range);
    }
}

SciQLopMultiPlotPanel* MultiPlotsVSpanCollection::panel()
{
    return dynamic_cast<SciQLopMultiPlotPanel*>(parent());
}

void MultiPlotsVSpanCollection::updateVisibleSpans(const SciQLopPlotRange& horizontal_range)
{
    for (auto& span : _spans)
    {
        if (span)
        {
            if (horizontal_range.intersects(span->range()))
            {
                span->show();
            }
            else
            {
                span->hide();
            }
        }
    }
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
        p, horizontal_range, color, read_only, horizontal_range.intersects(p->time_axis_range()),
        tool_tip, id);
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

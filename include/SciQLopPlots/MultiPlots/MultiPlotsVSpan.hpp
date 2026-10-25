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
#pragma once

#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"

#include "../Items/SciQLopVerticalSpan.hpp"
#include "SciQLopMultiPlotObject.hpp"
#include "SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"

class MultiPlotsVSpanCollection;

/*!
 * \brief The old name of a panel-wide vertical span: a SciQLopSpan at panel scope whose parts are
 * SciQLopVerticalSpans.
 *
 * Built by its deprecated constructor, MultiPlotsVSpanCollection::create_span and interactive
 * creation (span_created); the factories return plain SciQLopSpans. It keeps the collection's
 * range culling and show()/hide(); its Python properties are in bindings.xml. Plan B removes it.
 */
class MultiPlotsVerticalSpan : public SciQLopSpan
{
    Q_OBJECT
    friend class MultiPlotsVSpanCollection;

    bool m_shown = true; // the user's choice; the collection's culling hides on top of it
    bool m_in_range = true;

    //! Out-of-range culling, kept apart from set_visible: a span the user hid stays hidden in range.
    void set_in_range(bool in_range);

protected:
    bool _visible() const override { return m_shown && m_in_range; }
    void _set_visible(bool visible) override;
    // Needed by set_visible: a hide while culled reads as unchanged (the span is already hidden)
    // but must be remembered. It is class-wide because apply_change has no per-setter switch;
    // colour and selection re-applying when unchanged is harmless.
    bool _always_apply() const override { return true; }

public:
    MultiPlotsVerticalSpan(SciQLopMultiPlotPanel* panel, SciQLopPlotRange horizontal_range,
                           QColor color = QColor::fromRgb(100, 100, 100), bool read_only = false,
                           bool visible = true, const QString tool_tip = "", QString id = "")
            : SciQLopSpan(panel, SpanKind::Vertical, horizontal_range, SciQLopPlotRange(), color,
                          read_only, visible, tool_tip, Coordinates::Data, id, true)
            , m_shown(visible)
    {
    }

    inline void show() { set_visible(true); }

    inline void hide() { set_visible(false); }
};

class MultiPlotsVSpanCollection : public SciQLopMultiPlotObject
{
    Q_OBJECT
    QList<QPointer<MultiPlotsVerticalSpan>> _spans;
    SciQLopMultiPlotPanel* panel();

    void updateVisibleSpans(const SciQLopPlotRange& horizontal_range);

public:
    MultiPlotsVSpanCollection(SciQLopMultiPlotPanel* panel)
            : SciQLopMultiPlotObject(
                  SciQLopPlots::on_owner_thread(panel, "MultiPlotsVSpanCollection"))
    {
        updatePlotList(panel->plots());
        connect(panel, &SciQLopMultiPlotPanel::time_range_changed, this,
                &MultiPlotsVSpanCollection::updateVisibleSpans);
    }

    virtual ~MultiPlotsVSpanCollection() override { }

    QPointer<MultiPlotsVerticalSpan>
    create_span(SciQLopPlotRange horizontal_range, QColor color = QColor::fromRgb(100, 100, 100),
                 bool read_only = false, const QString tool_tip = "", const QString id = "");

    void delete_span(QPointer<MultiPlotsVerticalSpan> vspan);
    void delete_span(const QString& id);

    QList<QPointer<MultiPlotsVerticalSpan>> spans() const;

    QList<QPointer<MultiPlotsVerticalSpan>> spans_in_range(SciQLopPlotRange horizontal_range) const;

    QPointer<MultiPlotsVerticalSpan> span(const QString& id) const;
};

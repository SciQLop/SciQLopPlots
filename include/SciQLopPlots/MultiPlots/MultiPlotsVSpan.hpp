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

class MultiPlotsVerticalSpan : public SciQLopMultiPlotObject
{
    Q_OBJECT
    QList<QPointer<SciQLopVerticalSpan>> _spans;

    // Over a snapshot: a slot may delete this span, or a plot span that edits _spans.
    template <typename F>
    void for_each_span(F&& f)
    {
        const auto spans = _spans;
        for (const auto& span : spans)
            if (span)
                f(span.data());
    }
    SciQLopPlotRange _horizontal_range;
    bool m_selected = false;
    bool _lower_border_selected = false;
    bool _upper_border_selected = false;
    bool m_visible = true;
    bool m_in_range = true;
    bool _read_only = false;
    QColor m_color;
    QString _tool_tip;
    QString _id;

    void select_lower_border(bool selected);
    void select_upper_border(bool selected);

protected:
    virtual void addObject(SciQLopPlotInterface* plot) override;
    virtual void removeObject(SciQLopPlotInterface* plot) override;
    bool _selected() const override { return m_selected; }
    void _set_selected(bool selected) override;
    QColor _color() const override { return m_color; }
    void _set_color(const QColor& color) override;
    bool _visible() const override { return m_visible && m_in_range; }
    void _set_visible(bool visible) override;
    // A hide while culled reads as unchanged (the span is already hidden) but must be remembered.
    bool _always_apply() const override { return true; }

public:
    MultiPlotsVerticalSpan(SciQLopMultiPlotPanel* panel, SciQLopPlotRange horizontal_range,
                           QColor color = QColor::fromRgb(100, 100, 100), bool read_only = false,
                           bool visible = true, const QString tool_tip = "", QString id = "")
            : SciQLopMultiPlotObject(SciQLopPlots::on_owner_thread(panel, "MultiPlotsVerticalSpan"))
    {
        _horizontal_range = horizontal_range;
        m_color = color;
        m_visible = visible;
        _read_only = read_only;
        _tool_tip = tool_tip;
        if (id == "")
        {
            _id = QUuid::createUuid().toString();
        }
        else
        {
            _id = id;
        }
        updatePlotList(panel->plots());
    }

    virtual ~MultiPlotsVerticalSpan() override
    {
        for (auto span : _spans)
        {
            if (span)
            {
                delete span.data();
            }
        }
    }

    [[nodiscard]] inline QString id() const { return _id; }

    //! Out-of-range culling, kept apart from set_visible: a hidden span stays hidden in range.
    void set_in_range(bool in_range);

    void set_range(const SciQLopPlotRange horizontal_range);

    [[nodiscard]] inline SciQLopPlotRange range() const noexcept { return _horizontal_range; }

    inline void set_tool_tip(const QString& tool_tip)
    {
        SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
        if (_tool_tip != tool_tip)
        {
            for (auto span : _spans)
            {
                if (span)
                    span->set_tool_tip(tool_tip);
            }
            _tool_tip = tool_tip;
        }
    }

    [[nodiscard]] inline QString tool_tip() const noexcept { return _tool_tip; }

    inline void set_read_only(bool read_only)
    {
        SCIQLOP_ON_OWNER_THREAD(set_read_only(read_only));
        if (_read_only != read_only)
        {
            for (auto span : _spans)
            {
                if (span)
                    span->set_read_only(read_only);
            }
            _read_only = read_only;
        }
    }

    [[nodiscard]] inline bool read_only() const noexcept { return _read_only; }

    inline void show() { set_visible(true); }

    inline void hide() { set_visible(false); }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void delete_requested();
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

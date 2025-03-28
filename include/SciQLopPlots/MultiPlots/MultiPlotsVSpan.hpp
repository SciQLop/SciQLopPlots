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

#include "../Items/SciQLopVerticalSpan.hpp"
#include "SciQLopMultiPlotObject.hpp"
#include "SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"

class MultiPlotsVerticalSpan : public SciQLopMultiPlotObject
{
    Q_OBJECT
    QList<QPointer<SciQLopVerticalSpan>> _spans;
    SciQLopPlotRange _horizontal_range;
    bool _selected = false;
    bool _lower_border_selected = false;
    bool _upper_border_selected = false;
    bool _visible = true;
    bool _read_only = false;
    QColor _color;
    QString _tool_tip;

    void select_lower_border(bool selected);
    void select_upper_border(bool selected);

protected:
    virtual void addObject(SciQLopPlotInterface* plot) override;
    virtual void removeObject(SciQLopPlotInterface* plot) override;

public:

    MultiPlotsVerticalSpan(SciQLopMultiPlotPanel* panel, SciQLopPlotRange horizontal_range,
        QColor color = QColor(100, 100, 100)
            , bool read_only = false, bool visible = true, const QString tool_tip = "")
            : SciQLopMultiPlotObject(panel)
    {
        _horizontal_range = horizontal_range;
        _color = color;
        _visible = visible;
        _read_only = read_only;
        _tool_tip = tool_tip;
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


    void set_selected(bool selected);

    [[nodiscard]] inline bool is_selected() const noexcept { return _selected; }

    inline void set_color(const QColor& color)
    {
        if (_color != color)
        {
            for (auto span : _spans)
            {
                span->set_color(color);
            }
            _color = color;
        }
    }

    inline QColor get_color() const { return _color; }

    void set_range(const SciQLopPlotRange horizontal_range);

    [[nodiscard]] inline SciQLopPlotRange range() const noexcept { return _horizontal_range; }

    inline void set_visible(bool visible)
    {
        if (_visible != visible)
        {
            for (auto span : _spans)
            {
                span->set_visible(visible);
            }
            _visible = visible;
        }
    }

    [[nodiscard]] inline bool is_visible() const noexcept { return _visible; }

    inline void set_tool_tip(const QString& tool_tip)
    {
        if (_tool_tip != tool_tip)
        {
            for (auto span : _spans)
            {
                span->set_tool_tip(tool_tip);
            }
            _tool_tip = tool_tip;
        }
    }

    [[nodiscard]] inline QString get_tool_tip() const noexcept { return _tool_tip; }


    inline void set_read_only(bool read_only)
    {
        if (_read_only != read_only)
        {
            for (auto span : _spans)
            {
                span->set_read_only(read_only);
            }
            _read_only = read_only;
        }
    }

    [[nodiscard]] inline bool is_read_only() const noexcept { return _read_only; }

    inline void show() { set_visible(true); }
    inline void hide() { set_visible(false); }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void selection_changed(bool);
    Q_SIGNAL void delete_requested();
};

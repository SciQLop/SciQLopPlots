/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/ThreadGuard.hpp"

#include "../Items/SciQLopStraightLines.hpp"
#include "SciQLopMultiPlotObject.hpp"
#include "SciQLopMultiPlotPanel.hpp"

/*! One vertical line shown on every plot of a panel, including plots added later.
    Dragging it on any plot moves it on all of them. */
class MultiPlotsVerticalLine : public SciQLopMultiPlotObject
{
    Q_OBJECT
    QList<QPointer<SciQLopVerticalLine>> _lines;
    double _position;
    QColor _color;
    double _line_width = 1.0;
    bool _read_only;
    bool _visible;
    QString _tool_tip;

    template <typename F>
    void for_each_line(F&& f)
    {
        for (auto& line : _lines)
            if (line)
                f(line.data());
    }

protected:
    virtual void addObject(SciQLopPlotInterface* plot) override;
    virtual void removeObject(SciQLopPlotInterface* plot) override;

public:
    MultiPlotsVerticalLine(SciQLopMultiPlotPanel* panel, double position,
                           QColor color = QColor(100, 100, 100), bool read_only = false,
                           bool visible = true, const QString tool_tip = "")
            : SciQLopMultiPlotObject(SciQLopPlots::on_owner_thread(panel, "MultiPlotsVerticalLine"))
            , _position { position }
            , _color { color }
            , _read_only { read_only }
            , _visible { visible }
            , _tool_tip { tool_tip }
    {
        updatePlotList(panel->plots());
    }

    virtual ~MultiPlotsVerticalLine() override;

    void set_position(double position);
    [[nodiscard]] inline double position() const noexcept { return _position; }

    void set_color(const QColor& color);
    [[nodiscard]] inline QColor color() const noexcept { return _color; }

    void set_line_width(double width);
    [[nodiscard]] inline double line_width() const noexcept { return _line_width; }

    void set_visible(bool visible);
    [[nodiscard]] inline bool visible() const noexcept { return _visible; }

    void set_read_only(bool read_only);
    [[nodiscard]] inline bool read_only() const noexcept { return _read_only; }

    void set_tool_tip(const QString& tool_tip);
    [[nodiscard]] inline QString tool_tip() const noexcept { return _tool_tip; }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void position_changed(double new_position);
};

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
#include "SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp"

MultiPlotsVerticalLine::~MultiPlotsVerticalLine()
{
    for_each_line([](SciQLopVerticalLine* line) { delete line; });
}

void MultiPlotsVerticalLine::addObject(SciQLopPlotInterface* plot)
{
    if (auto scp = dynamic_cast<SciQLopPlot*>(plot); scp)
    {
        auto line = new SciQLopVerticalLine(scp, _position, !_read_only);
        // Owned by the plot so a plot destroyed directly takes its line along.
        line->setParent(scp);
        line->set_color(_color);
        line->set_line_width(_line_width);
        line->set_visible(_visible);
        line->set_tool_tip(_tool_tip);
        QObject::connect(line, &SciQLopVerticalLine::position_changed, this,
                         &MultiPlotsVerticalLine::set_position);
        _lines.append(line);
    }
}

void MultiPlotsVerticalLine::removeObject(SciQLopPlotInterface* plot)
{
    _lines.removeIf([](const QPointer<SciQLopVerticalLine>& line) { return line.isNull(); });
    for (int i = _lines.size() - 1; i >= 0; --i)
    {
        if (_lines[i]->parent() == plot)
        {
            delete _lines[i].data();
            _lines.removeAt(i);
        }
    }
}

void MultiPlotsVerticalLine::set_position(double position)
{
    SCIQLOP_ON_OWNER_THREAD(set_position(position));
    if (position == _position)
        return;
    // Store first: each line echoes the move back through position_changed.
    _position = position;
    for_each_line([position](SciQLopVerticalLine* line) { line->set_position(position); });
    Q_EMIT position_changed(position);
}

void MultiPlotsVerticalLine::set_color(const QColor& color)
{
    SCIQLOP_ON_OWNER_THREAD(set_color(color));
    _color = color;
    for_each_line([&color](SciQLopVerticalLine* line) { line->set_color(color); });
}

void MultiPlotsVerticalLine::set_line_width(double width)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_width(width));
    _line_width = width;
    for_each_line([width](SciQLopVerticalLine* line) { line->set_line_width(width); });
}

void MultiPlotsVerticalLine::set_visible(bool visible)
{
    SCIQLOP_ON_OWNER_THREAD(set_visible(visible));
    _visible = visible;
    for_each_line([visible](SciQLopVerticalLine* line) { line->set_visible(visible); });
}

void MultiPlotsVerticalLine::set_read_only(bool read_only)
{
    SCIQLOP_ON_OWNER_THREAD(set_read_only(read_only));
    _read_only = read_only;
    for_each_line([read_only](SciQLopVerticalLine* line) { line->set_movable(!read_only); });
}

void MultiPlotsVerticalLine::set_tool_tip(const QString& tool_tip)
{
    SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
    _tool_tip = tool_tip;
    for_each_line([&tool_tip](SciQLopVerticalLine* line) { line->set_tool_tip(tool_tip); });
}

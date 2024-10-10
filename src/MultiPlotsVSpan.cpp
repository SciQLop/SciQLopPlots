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
        for (auto span : _spans)
        {
            span->select_lower_border(selected);
        }
        replotAll();
        _lower_border_selected = selected;
    }
}

void MultiPlotsVerticalSpan::select_upper_border(bool selected)
{
    if (_upper_border_selected != selected)
    {
        for (auto span : _spans)
        {
            span->select_upper_border(selected);
        }
        replotAll();
        _upper_border_selected = selected;
    }
}

void MultiPlotsVerticalSpan::addObject(SciQLopPlotInterface* plot)
{
    if (auto scp = dynamic_cast<SciQLopPlot*>(plot); scp)
    {
        auto new_span = new SciQLopVerticalSpan(scp, _horizontal_range, true);
        new_span->set_selected(_selected);
        new_span->set_range(_horizontal_range);
        new_span->set_visible(_visible);
        new_span->set_color(_color);
        new_span->set_read_only(_read_only);
        new_span->set_tool_tip(_tool_tip);
        QObject::connect(new_span, &SciQLopVerticalSpan::range_changed, this,
                         &MultiPlotsVerticalSpan::set_range);
        QObject::connect(new_span, &SciQLopVerticalSpan::selectionChanged, this,
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
    if (auto scp = dynamic_cast<SciQLopPlot*>(plot); scp)
    {
        for (decltype(std::size(_spans)) i = 0UL; i < std::size(_spans); ++i)
        {
            if (_spans[i]->parentPlot() == scp->qcp_plot())
            {
                delete _spans[i];
                _spans.removeAt(i);
            }
        }
    }
}

void MultiPlotsVerticalSpan::set_selected(bool selected)
{
    if (_selected != selected)
    {
        for (auto span : _spans)
        {
            span->set_selected(selected);
        }
        replotAll();
        _selected = selected;
        Q_EMIT selection_changed(selected);
    }
}

void MultiPlotsVerticalSpan::set_range(const SciQLopPlotRange horizontal_range)
{
    if (horizontal_range != _horizontal_range)
    {
        for (auto span : _spans)
        {
            span->set_range(horizontal_range);
        }
        _horizontal_range = horizontal_range;
        Q_EMIT range_changed(horizontal_range);
    }
}

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

#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"
#include "SciQLopPlots/Plotables/SciQLopTimeline.hpp"

class QComboBox;
class QDoubleSpinBox;

//! Inspector panel for a timeline: style, lane height, editing and snapping.
// simplify: widgets are set from the timeline once, when the panel opens; changes made
// from Python while it is open don't show until it is reopened. Add change signals to
// SciQLopTimeline when a two-way panel is needed.
class SciQLopTimelineDelegate : public PropertyDelegateBase
{
    Q_OBJECT

    SciQLopTimeline* timeline() const { return as_type<SciQLopTimeline>(m_object); }

    QComboBox* m_snap = nullptr;
    QDoubleSpinBox* m_snapStep = nullptr;

    void add_style_row(SciQLopTimeline* tl);
    void add_stacking_rows(SciQLopTimeline* tl);
    void add_lane_height_row(SciQLopTimeline* tl);
    void add_editing_rows(SciQLopTimeline* tl);
    void add_snap_rows(SciQLopTimeline* tl);
    void apply_snap();

public:
    using compatible_type = SciQLopTimeline;
    SciQLopTimelineDelegate(SciQLopTimeline* object, QWidget* parent = nullptr);
    ~SciQLopTimelineDelegate() override = default;
};

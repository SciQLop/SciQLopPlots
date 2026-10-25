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

#include "SciQLopPlots/Items/SciQLopSpan.hpp"

#include <QPointer>

/*!
 * \brief The old name of a plot's horizontal span: a SciQLopSpan bounding the value axis.
 *
 * Built by its deprecated constructor; the factories return plain SciQLopSpans. It keeps the old
 * API SciQLop may call: the border selection. Plan B removes it
 * (docs/plans/2026-10-25-span-line-unification-design.md).
 */
class SciQLopHorizontalSpan : public SciQLopSpan
{
    Q_OBJECT

public:
    SciQLopHorizontalSpan(SciQLopPlot* plot, SciQLopPlotRange vertical_range,
                          QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false,
                          bool visible = true, const QString& tool_tip = "")
            : SciQLopSpan(plot, SpanKind::Horizontal, SciQLopPlotRange(), vertical_range, color,
                          read_only, visible, tool_tip, Coordinates::Data)
    {
    }

    //! Old API, public and bound today; never fired by user input (NeoQCP does not call it).
    //! Plan B removes it.
    void select_lower_border(bool selected)
    {
        const QPointer<SciQLopHorizontalSpan> alive(this);
        set_selected(selected);
        if (alive)
            Q_EMIT lower_border_selection_changed(selected);
    }

    void select_upper_border(bool selected)
    {
        const QPointer<SciQLopHorizontalSpan> alive(this);
        set_selected(selected);
        if (alive)
            Q_EMIT upper_border_selection_changed(selected);
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);
};

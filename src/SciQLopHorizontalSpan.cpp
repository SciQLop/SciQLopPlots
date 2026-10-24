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
#include <SciQLopPlots/SciQLopPlot.hpp>

#include "SciQLopPlots/Items/SciQLopHorizontalSpan.hpp"
#include "SciQLopPlots/constants.hpp"
#include "SciQLopPlots/SetThenEmit.hpp"

impl::HorizontalSpan::HorizontalSpan(QCustomPlot* plot, SciQLopPlotRange vertical_range,
                                      bool do_not_replot, bool immediate_replot)
    : QCPItemHSpan(plot), SpanBase(this)
{
    setLayer(Constants::LayersNames::Spans);
    setSelectable(true);
    setMovable(true);
    setRange(QCPRange(vertical_range.first, vertical_range.second));

    connect(this, &QCPItemHSpan::rangeChanged, this,
            [this](const QCPRange& r) {
                emit range_changed(SciQLopPlotRange(r.lower, r.upper));
            });

    connect(this, &QCPAbstractItem::deleteRequested, this,
            [this]() { emit delete_requested(); });

    if (!do_not_replot)
        replot(immediate_replot);
}

void impl::HorizontalSpan::set_range(const SciQLopPlotRange vertical_range)
{
    auto sorted = vertical_range.sorted();
    if (range() == sorted)
        return;
    if (SciQLopPlots::set_then_emit(
            this, [&] { setRange(QCPRange(sorted.first, sorted.second)); },
            [this] { emit range_changed(range()); }))
        replot();
}

SciQLopPlotRange impl::HorizontalSpan::range() const noexcept
{
    auto r = QCPItemHSpan::range();
    return SciQLopPlotRange(r.lower, r.upper);
}

void impl::HorizontalSpan::select_lower_border(bool selected)
{
    // setSelected() emits selectionChanged first, and a slot on it may delete this span.
    const QPointer<HorizontalSpan> self(this);
    setSelected(selected);
    if (self)
        emit lower_border_selection_changed(selected);
}

void impl::HorizontalSpan::select_upper_border(bool selected)
{
    // setSelected() emits selectionChanged first, and a slot on it may delete this span.
    const QPointer<HorizontalSpan> self(this);
    setSelected(selected);
    if (self)
        emit upper_border_selection_changed(selected);
}

SciQLopHorizontalSpan::SciQLopHorizontalSpan(SciQLopPlot* plot, SciQLopPlotRange vertical_range,
                                             QColor color, bool read_only, bool visible,
                                             const QString& tool_tip)
        : SciQLopRangeItemInterface { SciQLopPlots::item_plot(plot, "SciQLopHorizontalSpan") }
        , _impl { new impl::HorizontalSpan { plot->qcp_plot(), vertical_range, true } }
        , _base { static_cast<impl::SpanBase*>(_impl.data()) }
{
    set_color(color);
    set_read_only(read_only);
    set_visible(visible);
    set_tool_tip(tool_tip);
    _impl->set_borders_color(color.lighter());

    connect(_impl.data(), &impl::HorizontalSpan::range_changed, this,
        &SciQLopHorizontalSpan::range_changed);
    _follow_selection(_impl.data());
    connect(_impl.data(), &impl::HorizontalSpan::lower_border_selection_changed, this,
        &SciQLopHorizontalSpan::lower_border_selection_changed);
    connect(_impl.data(), &impl::HorizontalSpan::upper_border_selection_changed, this,
        &SciQLopHorizontalSpan::upper_border_selection_changed);
    connect(_impl.data(), &impl::HorizontalSpan::delete_requested, this,
        &SciQLopHorizontalSpan::delete_requested);

    _impl->replot();
}

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

#include "SciQLopPlots/Items/SciQLopRectangularSpan.hpp"
#include "SciQLopPlots/constants.hpp"

impl::RectangularSpan::RectangularSpan(QCustomPlot* plot, SciQLopPlotRange key_range,
                                        SciQLopPlotRange value_range, bool do_not_replot,
                                        bool immediate_replot)
    : QCPItemRSpan(plot)
{
    setLayer(Constants::LayersNames::Spans);
    setSelectable(true);
    setMovable(true);
    setKeyRange(QCPRange(key_range.first, key_range.second));
    setValueRange(QCPRange(value_range.first, value_range.second));

    connect(this, &QCPItemRSpan::keyRangeChanged, this,
            [this](const QCPRange& r) {
                emit key_range_changed(SciQLopPlotRange(r.lower, r.upper));
            });

    connect(this, &QCPItemRSpan::valueRangeChanged, this,
            [this](const QCPRange& r) {
                emit value_range_changed(SciQLopPlotRange(r.lower, r.upper));
            });

    connect(this, &QCPAbstractItem::deleteRequested, this,
            [this]() { emit delete_requested(); });

    if (!do_not_replot)
        replot(immediate_replot);
}

void impl::RectangularSpan::set_visible(bool visible) { setVisible(visible); }

void impl::RectangularSpan::set_key_range(const SciQLopPlotRange range)
{
    auto sorted = range.sorted();
    if (key_range() == sorted)
        return;
    setKeyRange(QCPRange(sorted.first, sorted.second));
    replot();
}

SciQLopPlotRange impl::RectangularSpan::key_range() const noexcept
{
    auto r = QCPItemRSpan::keyRange();
    return SciQLopPlotRange(r.lower, r.upper);
}

void impl::RectangularSpan::set_value_range(const SciQLopPlotRange range)
{
    auto sorted = range.sorted();
    if (value_range() == sorted)
        return;
    setValueRange(QCPRange(sorted.first, sorted.second));
    replot();
}

SciQLopPlotRange impl::RectangularSpan::value_range() const noexcept
{
    auto r = QCPItemRSpan::valueRange();
    return SciQLopPlotRange(r.lower, r.upper);
}

void impl::RectangularSpan::set_color(const QColor& color)
{
    setBrush(QBrush(color, Qt::SolidPattern));
    setSelectedBrush(QBrush(
        QColor(255 - color.red(), 255 - color.green(), 255 - color.blue(), color.alpha()),
        Qt::SolidPattern));
    setPen(QPen(Qt::NoPen));
    setSelectedPen(QPen(Qt::NoPen));
}

QColor impl::RectangularSpan::color() const noexcept { return brush().color(); }

void impl::RectangularSpan::set_borders_color(const QColor& color)
{
    setBorderPen(QPen(QBrush(color, Qt::SolidPattern), 3));
}

QColor impl::RectangularSpan::borders_color() const noexcept
{
    return borderPen().color();
}

void impl::RectangularSpan::set_borders_tool_tip(const QString& tool_tip)
{
    SciQlopItemWithToolTip::setToolTip(tool_tip);
}

SciQLopRectangularSpan::SciQLopRectangularSpan(SciQLopPlot* plot, SciQLopPlotRange key_range,
    SciQLopPlotRange value_range, QColor color, bool read_only, bool visible,
    const QString& tool_tip)
        : SciQLopMovableItemInterface { plot }
        , _impl { new impl::RectangularSpan { plot->qcp_plot(), key_range, value_range, true } }
{
    set_color(color);
    set_read_only(read_only);
    set_visible(visible);
    set_tool_tip(tool_tip);
    _impl->set_borders_color(color.lighter());

    connect(_impl.data(), &impl::RectangularSpan::key_range_changed, this,
        &SciQLopRectangularSpan::key_range_changed);
    connect(_impl.data(), &impl::RectangularSpan::value_range_changed, this,
        &SciQLopRectangularSpan::value_range_changed);
    connect(_impl.data(), &QCPAbstractItem::selectionChanged, this,
        &SciQLopRectangularSpan::selectionChanged);
    connect(_impl.data(), &impl::RectangularSpan::delete_requested, this,
        &SciQLopRectangularSpan::delete_requested);

    _impl->replot();
}

/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Items/SciQLopVerticalSpan.hpp"
#include "SciQLopPlots/constants.hpp"

impl::VerticalSpan::VerticalSpan(QCustomPlot* plot, SciQLopPlotRange horizontal_range,
                                  Coordinates coordinates,
                                  bool do_not_replot, bool immediate_replot)
    : QCPItemVSpan(plot), SpanBase(this), m_coordinates(coordinates)
{
    setLayer(Constants::LayersNames::Spans);
    setSelectable(true);
    setMovable(true);
    if (coordinates == Coordinates::Pixels)
    {
        lowerEdge->setTypeX(QCPItemPosition::ptAbsolute);
        upperEdge->setTypeX(QCPItemPosition::ptAbsolute);
    }
    setRange(QCPRange(horizontal_range.first, horizontal_range.second));

    connect(this, &QCPItemVSpan::rangeChanged, this,
            [this](const QCPRange& r) {
                emit range_changed(SciQLopPlotRange(r.lower, r.upper));
            });

    connect(this, &QCPAbstractItem::deleteRequested, this,
            [this]() { emit delete_requested(); });

    if (!do_not_replot)
        replot(immediate_replot);
}

void impl::VerticalSpan::set_range(const SciQLopPlotRange horizontal_range)
{
    auto sorted = horizontal_range.sorted();
    if (range() == sorted)
        return;
    setRange(QCPRange(sorted.first, sorted.second));
    replot();
}

SciQLopPlotRange impl::VerticalSpan::range() const noexcept
{
    auto r = QCPItemVSpan::range();
    return SciQLopPlotRange(r.lower, r.upper);
}

void impl::VerticalSpan::set_coordinates(Coordinates coordinates)
{
    if (m_coordinates == coordinates)
        return;
    auto current = range();
    if (coordinates == Coordinates::Pixels)
    {
        auto pr = pixel_range();
        lowerEdge->setTypeX(QCPItemPosition::ptAbsolute);
        upperEdge->setTypeX(QCPItemPosition::ptAbsolute);
        setRange(QCPRange(pr.first, pr.second));
    }
    else
    {
        lowerEdge->setTypeX(QCPItemPosition::ptPlotCoords);
        upperEdge->setTypeX(QCPItemPosition::ptPlotCoords);
        auto* keyAxis = lowerEdge->keyAxis();
        if (keyAxis)
        {
            double lower = keyAxis->pixelToCoord(current.first);
            double upper = keyAxis->pixelToCoord(current.second);
            setRange(QCPRange(lower, upper));
        }
    }
    m_coordinates = coordinates;
    replot();
}

SciQLopPlotRange impl::VerticalSpan::pixel_range() const noexcept
{
    auto* keyAxis = lowerEdge->keyAxis();
    if (!keyAxis)
        return {};
    return SciQLopPlotRange(keyAxis->coordToPixel(lowerEdge->coords().x()),
                            keyAxis->coordToPixel(upperEdge->coords().x()));
}

void impl::VerticalSpan::set_pixel_range(const SciQLopPlotRange& px_range)
{
    auto* keyAxis = lowerEdge->keyAxis();
    if (!keyAxis)
        return;
    set_range(SciQLopPlotRange(keyAxis->pixelToCoord(px_range.first),
                               keyAxis->pixelToCoord(px_range.second)));
}

void impl::VerticalSpan::select_lower_border(bool selected)
{
    setSelected(selected);
    emit lower_border_selection_changed(selected);
}

void impl::VerticalSpan::select_upper_border(bool selected)
{
    setSelected(selected);
    emit upper_border_selection_changed(selected);
}

SciQLopVerticalSpan::SciQLopVerticalSpan(SciQLopPlot* plot, SciQLopPlotRange horizontal_range,
    QColor color, bool read_only, bool visible, const QString& tool_tip,
    Coordinates coordinates)
        : SciQLopRangeItemInterface { plot }
        , _impl { new impl::VerticalSpan { plot->qcp_plot(), horizontal_range, coordinates, true } }
        , _base { static_cast<impl::SpanBase*>(_impl.data()) }
{
    set_color(color);
    set_read_only(read_only);
    set_visible(visible);
    set_tool_tip(tool_tip);
    _impl->set_borders_color(color.lighter());

    connect(_impl.data(), &impl::VerticalSpan::range_changed, this,
        &SciQLopVerticalSpan::range_changed);
    connect(_impl.data(), &QCPAbstractItem::selectionChanged, this,
        &SciQLopVerticalSpan::selectionChanged);
    connect(_impl.data(), &impl::VerticalSpan::lower_border_selection_changed, this,
        &SciQLopVerticalSpan::lower_border_selection_changed);
    connect(_impl.data(), &impl::VerticalSpan::upper_border_selection_changed, this,
        &SciQLopVerticalSpan::upper_border_selection_changed);
    connect(_impl.data(), &impl::VerticalSpan::delete_requested, this,
        &SciQLopVerticalSpan::delete_requested);

    _impl->replot();
}

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
#pragma once
#include "SciQLopPlots/Items/SciQLopSpan.hpp"

#include "SciQLopPlots/Items/SciQLopPlotItem.hpp"
#include "SciQLopPlots/Items/impl/SpanTargets.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"

#include <QPointer>
#include <QUuid>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using impl::SpanAxis;

namespace
{
const char* axis_name(SpanAxis axis)
{
    return axis == SpanAxis::Key ? "key" : "value";
}

QString id_or_uuid(const QString& id)
{
    return id.isEmpty() ? QUuid::createUuid().toString() : id;
}
} // namespace

SpanKind SciQLopPlots::span_kind(bool bounds_key, bool bounds_value)
{
    if (bounds_key && bounds_value)
        return SpanKind::Rectangular;
    if (bounds_key)
        return SpanKind::Vertical;
    if (bounds_value)
        return SpanKind::Horizontal;
    throw std::invalid_argument("span: give key_range, value_range or both");
}

SciQLopSpan::SciQLopSpan(SciQLopPlot* plot, SpanKind kind, const SciQLopPlotRange& key_range,
                         const SciQLopPlotRange& value_range, const QColor& color, bool read_only,
                         bool visible, const QString& tool_tip, Coordinates coordinates,
                         const QString& id)
        : SciQLopDrawnObject { SciQLopPlots::item_plot(plot, "SciQLopSpan") }
        , m_kind { kind }
        , m_id { id_or_uuid(id) }
        , m_target { std::make_unique<impl::PlotSpanTarget>(this, plot, kind, key_range,
                                                            value_range, coordinates) }
{
    m_target->set_color(color);
    m_target->set_borders_color(color.lighter());
    m_target->set_read_only(read_only);
    m_target->set_visible(visible);
    m_target->set_tool_tip(tool_tip);
}

SciQLopSpan::SciQLopSpan(SciQLopMultiPlotPanel* panel, SpanKind kind,
                         const SciQLopPlotRange& key_range, const SciQLopPlotRange& value_range,
                         const QColor& color, bool read_only, bool visible,
                         const QString& tool_tip, Coordinates coordinates, const QString& id,
                         bool legacy_parts)
        : SciQLopDrawnObject { impl::panel_host(panel, "SciQLopSpan") }
        , m_kind { kind }
        , m_id { id_or_uuid(id) }
        , m_target { std::make_unique<impl::PanelSpanTarget>(
              this, panel, kind,
              impl::SpanState { .key_range = key_range.sorted(),
                                .value_range = value_range.sorted(),
                                .coordinates = coordinates,
                                .read_only = read_only,
                                .tool_tip = tool_tip,
                                .color = color,
                                .borders_color = color.lighter(),
                                .visible = visible },
              legacy_parts) }
{
}

SciQLopSpan::~SciQLopSpan() = default;

bool SciQLopSpan::bounds(SpanAxis axis) const noexcept
{
    return m_kind == SpanKind::Rectangular
        || (m_kind == SpanKind::Vertical) == (axis == SpanAxis::Key);
}

void SciQLopSpan::require_bounded(SpanAxis axis, const char* what) const
{
    if (!bounds(axis))
        throw std::invalid_argument(std::string(what) + ": this span does not bound the "
                                    + axis_name(axis) + " axis");
}

SpanAxis SciQLopSpan::one_axis(const char* what) const
{
    if (m_kind == SpanKind::Rectangular)
        throw std::invalid_argument(std::string(what)
                                    + ": a rectangular span has two ranges; use key_range() "
                                      "and value_range()");
    return m_kind == SpanKind::Vertical ? SpanAxis::Key : SpanAxis::Value;
}

void SciQLopSpan::notify_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    const QPointer<SciQLopSpan> alive(this);
    if (axis == SpanAxis::Key)
        Q_EMIT key_range_changed(range);
    else
        Q_EMIT value_range_changed(range);
    if (alive && m_kind != SpanKind::Rectangular)
        Q_EMIT range_changed(range);
}

SpanKind SciQLopSpan::kind() const noexcept
{
    return m_kind;
}

QObject* SciQLopSpan::scope() const
{
    return parent();
}

bool SciQLopSpan::is_panel_part() const
{
    return SciQLopPlots::is_panel_part(this);
}

QString SciQLopSpan::id() const
{
    return m_id;
}

std::optional<SciQLopPlotRange> SciQLopSpan::key_range() const
{
    if (!bounds(SpanAxis::Key))
        return std::nullopt;
    return m_target->range(SpanAxis::Key);
}

std::optional<SciQLopPlotRange> SciQLopSpan::value_range() const
{
    if (!bounds(SpanAxis::Value))
        return std::nullopt;
    return m_target->range(SpanAxis::Value);
}

// The kind is checked before the hop: a throw from the queued call would escape the event loop.
void SciQLopSpan::set_key_range(const SciQLopPlotRange& range)
{
    require_bounded(SpanAxis::Key, "set_key_range");
    SCIQLOP_ON_OWNER_THREAD(set_key_range(range));
    m_target->set_range(SpanAxis::Key, range.sorted());
}

void SciQLopSpan::set_value_range(const SciQLopPlotRange& range)
{
    require_bounded(SpanAxis::Value, "set_value_range");
    SCIQLOP_ON_OWNER_THREAD(set_value_range(range));
    m_target->set_range(SpanAxis::Value, range.sorted());
}

SciQLopPlotRange SciQLopSpan::range() const
{
    return m_target->range(one_axis("range"));
}

void SciQLopSpan::set_range(const SciQLopPlotRange& range)
{
    const auto axis = one_axis("set_range");
    SCIQLOP_ON_OWNER_THREAD(set_range(range));
    m_target->set_range(axis, range.sorted());
}

Coordinates SciQLopSpan::coordinates() const
{
    return m_target->coordinates();
}

void SciQLopSpan::set_coordinates(Coordinates coordinates)
{
    SCIQLOP_ON_OWNER_THREAD(set_coordinates(coordinates));
    if (m_target->coordinates() == coordinates)
        return;
    std::vector<std::pair<SpanAxis, SciQLopPlotRange>> before;
    for (const auto axis : impl::bounded_axes(m_kind))
        before.emplace_back(axis, m_target->range(axis));
    // A part's slot may delete a panel span while its target writes.
    const QPointer<SciQLopSpan> alive(this);
    m_target->set_coordinates(coordinates);
    if (!alive)
        return;
    Q_EMIT coordinates_changed(coordinates);
    for (const auto& [axis, old] : before)
    {
        if (!alive)
            return;
        if (const auto now = m_target->range(axis); now != old)
            notify_range(axis, now);
    }
}

SciQLopPlotRange SciQLopSpan::pixel_range() const
{
    return m_target->pixel_range(one_axis("pixel_range"));
}

void SciQLopSpan::_set_pixel_range(SpanAxis axis, const SciQLopPlotRange& pixels)
{
    SCIQLOP_ON_OWNER_THREAD(_set_pixel_range(axis, pixels));
    m_target->set_range(axis, m_target->to_range(axis, pixels));
}

SciQLopPlotRange SciQLopSpan::pixel_key_range() const
{
    require_bounded(SpanAxis::Key, "pixel_key_range");
    return m_target->pixel_range(SpanAxis::Key);
}

SciQLopPlotRange SciQLopSpan::pixel_value_range() const
{
    require_bounded(SpanAxis::Value, "pixel_value_range");
    return m_target->pixel_range(SpanAxis::Value);
}

QColor SciQLopSpan::borders_color() const
{
    return m_target->borders_color();
}

void SciQLopSpan::set_borders_color(const QColor& color)
{
    SCIQLOP_ON_OWNER_THREAD(set_borders_color(color));
    m_target->set_borders_color(color);
}

double SciQLopSpan::line_width() const
{
    return m_target->line_width();
}

void SciQLopSpan::set_line_width(double width)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_width(width));
    m_target->set_line_width(width);
}

Qt::PenStyle SciQLopSpan::line_style() const
{
    return m_target->line_style();
}

void SciQLopSpan::set_line_style(Qt::PenStyle style)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_style(style));
    m_target->set_line_style(style);
}

bool SciQLopSpan::read_only() const
{
    return m_target->read_only();
}

void SciQLopSpan::set_read_only(bool read_only)
{
    SCIQLOP_ON_OWNER_THREAD(set_read_only(read_only));
    if (m_target->read_only() == read_only)
        return;
    const QPointer<SciQLopSpan> alive(this);
    m_target->set_read_only(read_only);
    if (alive)
        Q_EMIT read_only_changed(read_only);
}

QString SciQLopSpan::tool_tip() const
{
    return m_target->tool_tip();
}

void SciQLopSpan::set_tool_tip(const QString& tool_tip)
{
    SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
    if (m_target->tool_tip() == tool_tip)
        return;
    const QPointer<SciQLopSpan> alive(this);
    m_target->set_tool_tip(tool_tip);
    if (alive)
        Q_EMIT tool_tip_changed(tool_tip);
}

bool SciQLopSpan::_visible() const
{
    return m_target->visible();
}

void SciQLopSpan::_set_visible(bool visible)
{
    m_target->set_visible(visible);
}

bool SciQLopSpan::_selected() const
{
    return m_target->selected();
}

void SciQLopSpan::_set_selected(bool selected)
{
    m_target->set_selected(selected);
}

QColor SciQLopSpan::_color() const
{
    return m_target->color();
}

void SciQLopSpan::_set_color(const QColor& color)
{
    m_target->set_color(color);
}

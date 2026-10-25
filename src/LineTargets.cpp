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
#include "SciQLopPlots/Items/impl/LineTargets.hpp"

#include "SciQLopPlots/helpers.hpp"

#include <limits>
#include <utility>

namespace impl
{
PlotLineTarget::PlotLineTarget(SciQLopStraightLine* line, SciQLopPlot* plot, double position,
                               bool movable, Coordinates coordinates, Qt::Orientation orientation)
        : m_line { new StraightLine(plot->qcp_plot(), position, movable, coordinates, orientation) }
{
    QObject::connect(m_line.data(), &StraightLine::moved, line,
                     &SciQLopStraightLine::position_changed);
    QObject::connect(m_line.data(), &QCPAbstractItem::selectionChanged, line,
                     [line](bool selected) { line->_notify_selected(selected); });
    QObject::connect(m_line.data(), &QCPAbstractItem::deleteRequested, line,
                     &SciQLopStraightLine::delete_requested);
}

PlotLineTarget::~PlotLineTarget()
{
    qcp_item_remove(m_line);
}

double PlotLineTarget::position() const
{
    return m_line ? m_line->position() : std::numeric_limits<double>::quiet_NaN();
}

void PlotLineTarget::set_position(double position)
{
    if (m_line)
        m_line->set_position(position);
}

Coordinates PlotLineTarget::coordinates() const
{
    return m_line ? m_line->coordinates() : Coordinates::Data;
}

void PlotLineTarget::set_coordinates(Coordinates coordinates)
{
    if (m_line)
        m_line->set_coordinates(coordinates);
}

bool PlotLineTarget::read_only() const
{
    return !m_line || !m_line->movable();
}

void PlotLineTarget::set_read_only(bool read_only)
{
    if (m_line)
        m_line->setMovable(!read_only);
}

QString PlotLineTarget::tool_tip() const
{
    return m_line ? m_line->tooltip() : QString();
}

void PlotLineTarget::set_tool_tip(const QString& tool_tip)
{
    if (m_line)
        m_line->setToolTip(tool_tip);
}

QColor PlotLineTarget::color() const
{
    return m_line ? m_line->color() : QColor();
}

void PlotLineTarget::set_color(const QColor& color)
{
    if (m_line)
        m_line->set_color(color);
}

double PlotLineTarget::line_width() const
{
    return m_line ? m_line->line_width() : std::numeric_limits<double>::quiet_NaN();
}

void PlotLineTarget::set_line_width(double width)
{
    if (m_line)
        m_line->set_line_width(width);
}

Qt::PenStyle PlotLineTarget::line_style() const
{
    return m_line ? m_line->line_style() : Qt::SolidLine;
}

void PlotLineTarget::set_line_style(Qt::PenStyle style)
{
    if (m_line)
        m_line->set_line_style(style);
}

bool PlotLineTarget::visible() const
{
    return m_line && m_line->visible();
}

void PlotLineTarget::set_visible(bool visible)
{
    if (!m_line)
        return;
    m_line->setVisible(visible);
    m_line->replot();
}

bool PlotLineTarget::selected() const
{
    return m_line && m_line->selected();
}

void PlotLineTarget::set_selected(bool selected)
{
    if (!m_line)
        return;
    m_line->setSelected(selected);
    m_line->replot();
}

std::optional<double> PlotLineTarget::min_value() const
{
    return m_line ? m_line->min_value() : std::nullopt;
}

void PlotLineTarget::set_min_value(std::optional<double> min)
{
    if (!m_line)
        return;
    if (min)
        m_line->set_min_value(*min);
    else
        m_line->clear_min_value();
}

std::optional<double> PlotLineTarget::max_value() const
{
    return m_line ? m_line->max_value() : std::nullopt;
}

void PlotLineTarget::set_max_value(std::optional<double> max)
{
    if (!m_line)
        return;
    if (max)
        m_line->set_max_value(*max);
    else
        m_line->clear_max_value();
}

PanelLineTarget::PanelLineTarget(SciQLopStraightLine* line, SciQLopMultiPlotPanel* panel,
                                 LineState state)
        : m_line { line }
        , m_state { std::move(state) }
        , m_parts { line, [this](SciQLopPlot* plot) { return make_part(plot); } }
{
    m_parts.follow(panel);
}

SciQLopStraightLine* PanelLineTarget::make_part(SciQLopPlot* plot)
{
    auto* part = new SciQLopStraightLine(plot, m_state.position, !m_state.read_only,
                                         m_state.coordinates, m_state.orientation);
    if (m_state.min_value)
        part->set_min_value(*m_state.min_value);
    if (m_state.max_value)
        part->set_max_value(*m_state.max_value);
    part->set_color(m_state.color);
    part->set_line_width(m_state.line_width);
    part->set_line_style(m_state.line_style);
    part->set_tool_tip(m_state.tool_tip);
    part->set_visible(m_state.visible);
    part->set_selected(m_state.selected);
    follow(part);
    return part;
}

// A part's own edits (a drag, a click, the Delete key) flow up; the line writes them back out to
// every part, whose echoes writing() swallows.
void PanelLineTarget::follow(SciQLopStraightLine* part)
{
    QObject::connect(part, &SciQLopStraightLine::position_changed, m_line,
                     [this](double position)
                     {
                         if (!m_parts.writing())
                             set_position(position);
                     });
    QObject::connect(part, &SciQLopDrawnObject::selection_changed, m_line,
                     [this](bool selected)
                     {
                         if (!m_parts.writing())
                             m_line->set_selected(selected);
                     });
    QObject::connect(part, &SciQLopStraightLine::delete_requested, m_line,
                     &SciQLopStraightLine::delete_requested);
}

template <typename T, typename Setter>
void PanelLineTarget::write(T& field, const T& value, Setter setter)
{
    field = value;
    (void)m_parts.write([value, setter](SciQLopStraightLine* part) { (part->*setter)(value); });
}

double PanelLineTarget::position() const
{
    return m_state.position;
}

void PanelLineTarget::set_position(double position)
{
    // The min/max clamps are data values: meaningless against a pixel offset.
    const bool data = m_state.coordinates == Coordinates::Data;
    position = clamp_line_position(position, data ? m_state.min_value : std::nullopt,
                                   data ? m_state.max_value : std::nullopt);
    if (position == m_state.position)
        return;
    m_state.position = position;
    if (m_parts.write([position](SciQLopStraightLine* part) { part->set_position(position); }))
        Q_EMIT m_line->position_changed(position);
}

Coordinates PanelLineTarget::coordinates() const
{
    return m_state.coordinates;
}

// simplify: each part converts on its own plot, then the first part's number becomes the line's
// and every part takes it; with no plot yet the number stays. A position per plot would lift it.
void PanelLineTarget::set_coordinates(Coordinates coordinates)
{
    m_state.coordinates = coordinates;
    if (!m_parts.write([coordinates](SciQLopStraightLine* part)
                       { part->set_coordinates(coordinates); }))
        return;
    auto* first = m_parts.first();
    if (!first)
        return;
    m_state.position = first->position();
    (void)m_parts.write([position = m_state.position](SciQLopStraightLine* part)
                        { part->set_position(position); });
}

bool PanelLineTarget::read_only() const
{
    return m_state.read_only;
}

void PanelLineTarget::set_read_only(bool read_only)
{
    write(m_state.read_only, read_only, &SciQLopStraightLine::set_read_only);
}

QString PanelLineTarget::tool_tip() const
{
    return m_state.tool_tip;
}

void PanelLineTarget::set_tool_tip(const QString& tool_tip)
{
    write(m_state.tool_tip, tool_tip, &SciQLopStraightLine::set_tool_tip);
}

QColor PanelLineTarget::color() const
{
    return m_state.color;
}

void PanelLineTarget::set_color(const QColor& color)
{
    write(m_state.color, color, &SciQLopDrawnObject::set_color);
}

double PanelLineTarget::line_width() const
{
    return m_state.line_width;
}

void PanelLineTarget::set_line_width(double width)
{
    write(m_state.line_width, width, &SciQLopStraightLine::set_line_width);
}

Qt::PenStyle PanelLineTarget::line_style() const
{
    return m_state.line_style;
}

void PanelLineTarget::set_line_style(Qt::PenStyle style)
{
    write(m_state.line_style, style, &SciQLopStraightLine::set_line_style);
}

bool PanelLineTarget::visible() const
{
    return m_state.visible;
}

void PanelLineTarget::set_visible(bool visible)
{
    write(m_state.visible, visible, &SciQLopDrawnObject::set_visible);
}

bool PanelLineTarget::selected() const
{
    return m_state.selected;
}

void PanelLineTarget::set_selected(bool selected)
{
    write(m_state.selected, selected, &SciQLopDrawnObject::set_selected);
}

std::optional<double> PanelLineTarget::min_value() const
{
    return m_state.min_value;
}

void PanelLineTarget::set_min_value(std::optional<double> min)
{
    m_state.min_value = min;
    (void)m_parts.write(
        [min](SciQLopStraightLine* part)
        {
            if (min)
                part->set_min_value(*min);
            else
                part->clear_min_value();
        });
}

std::optional<double> PanelLineTarget::max_value() const
{
    return m_state.max_value;
}

void PanelLineTarget::set_max_value(std::optional<double> max)
{
    m_state.max_value = max;
    (void)m_parts.write(
        [max](SciQLopStraightLine* part)
        {
            if (max)
                part->set_max_value(*max);
            else
                part->clear_max_value();
        });
}
} // namespace impl

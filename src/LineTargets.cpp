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
} // namespace impl

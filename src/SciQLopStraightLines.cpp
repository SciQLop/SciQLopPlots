/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Items/SciQLopStraightLines.hpp"

#include "SciQLopPlots/Items/impl/LineTargets.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"

#include <QPointer>

void StraightLine::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    if (!_movable || event->buttons() != Qt::LeftButton)
        return;
    const QPointer<StraightLine> self(this);
    if (const auto position = dragged_position(event->position()))
        set_position(*position);
    // set_position emits: a slot may have deleted this line.
    if (!self)
        return;
    _last_position = event->position();
    event->accept();
}

std::optional<double> StraightLine::dragged_position(const QPointF& mouse) const
{
    const bool vertical = m_orientation == Qt::Orientation::Vertical;
    if (m_coordinates == Coordinates::Pixels)
    {
        const auto* rect = point1->axisRect();
        if (!rect)
            return std::nullopt;
        const QPointF fromAxisRect = mouse - rect->topLeft();
        return vertical ? fromAxisRect.x() : fromAxisRect.y();
    }
    const auto* axis = vertical ? point1->keyAxis() : point1->valueAxis();
    if (!axis)
        return std::nullopt;
    return axis->pixelToCoord(vertical ? mouse.x() : mouse.y());
}

void StraightLine::move(double dx, double dy)
{
    auto pos = position();
    if (m_orientation == Qt::Orientation::Vertical)
        set_position(pos + dx);
    else
        set_position(pos + dy);
}

void StraightLine::place(double pos)
{
    if (m_orientation == Qt::Orientation::Vertical)
    {
        this->point1->setCoords(pos, 0);
        this->point2->setCoords(pos, 1);
    }
    else
    {
        this->point1->setCoords(0, pos);
        this->point2->setCoords(1, pos);
    }
}

void StraightLine::set_position(double pos)
{
    pos = _clamp(pos);
    if (position() == pos)
        return;
    place(pos);
    const QPointer<StraightLine> self(this);
    emit moved(pos);
    if (self)
        this->replot();
}

void StraightLine::set_coordinates(Coordinates coordinates)
{
    if (coordinates == m_coordinates)
        return;
    const bool vertical = m_orientation == Qt::Orientation::Vertical;
    const QPointF pixel = point1->pixelPosition();
    double position = 0.;
    if (coordinates == Coordinates::Pixels)
    {
        const auto* rect = point1->axisRect();
        if (!rect)
            return;
        position = vertical ? pixel.x() - rect->left() : pixel.y() - rect->top();
    }
    else
    {
        const auto* axis = vertical ? point1->keyAxis() : point1->valueAxis();
        if (!axis)
            return;
        position = axis->pixelToCoord(vertical ? pixel.x() : pixel.y());
    }
    // Pixels are measured from the plot area: ptAbsolute's origin sits in the axis margins.
    const auto type = coordinates == Coordinates::Pixels ? QCPItemPosition::ptAxisRectAbsolute
                                                         : QCPItemPosition::ptPlotCoords;
    point1->setType(type);
    point2->setType(type);
    m_coordinates = coordinates;
    place(position);
    replot();
}

double StraightLine::position() const
{
    if (m_orientation == Qt::Orientation::Vertical)
    {
        return this->point1->key();
    }
    else
    {
        return this->point1->value();
    }
}

void StraightLine::set_color(const QColor &color)
{
    auto pen = this->pen();
    pen.setColor(color);
    this->setPen(pen);
    this->replot();
}

QColor StraightLine::color() const
{
    return this->pen().color();
}

void StraightLine::set_line_width(double width)
{
    auto pen = this->pen();
    pen.setWidthF(width);
    this->setPen(pen);
    this->replot();
}

double StraightLine::line_width() const
{
    return this->pen().widthF();
}

void StraightLine::set_line_style(Qt::PenStyle style)
{
    auto pen = this->pen();
    pen.setStyle(style);
    this->setPen(pen);
    this->replot();
}

Qt::PenStyle StraightLine::line_style() const
{
    return this->pen().style();
}

SciQLopStraightLine::SciQLopStraightLine(SciQLopPlot* plot, const double position,
                                         const bool movable, Coordinates coordinates,
                                         Qt::Orientation orientation)
        : SciQLopDrawnObject { SciQLopPlots::item_plot(plot, "SciQLopStraightLine") }
        , m_orientation { orientation }
        , m_target { std::make_unique<impl::PlotLineTarget>(this, plot, position, movable,
                                                            coordinates, orientation) }
{
}

SciQLopStraightLine::~SciQLopStraightLine() = default;

Qt::Orientation SciQLopStraightLine::orientation() const noexcept
{
    return m_orientation;
}

QObject* SciQLopStraightLine::scope() const
{
    return parent();
}

bool SciQLopStraightLine::is_panel_part() const
{
    return SciQLopPlots::is_panel_part(this);
}

void SciQLopStraightLine::set_position(double pos)
{
    SCIQLOP_ON_OWNER_THREAD(set_position(pos));
    m_target->set_position(pos);
}

double SciQLopStraightLine::position() const
{
    return m_target->position();
}

Coordinates SciQLopStraightLine::coordinates() const
{
    return m_target->coordinates();
}

void SciQLopStraightLine::set_coordinates(Coordinates coordinates)
{
    SCIQLOP_ON_OWNER_THREAD(set_coordinates(coordinates));
    if (m_target->coordinates() == coordinates)
        return;
    const double before = m_target->position();
    // A part's slot may delete a panel line while its target writes.
    const QPointer<SciQLopStraightLine> alive(this);
    m_target->set_coordinates(coordinates);
    if (!alive)
        return;
    Q_EMIT coordinates_changed(coordinates);
    if (!alive)
        return;
    if (const double now = m_target->position(); now != before)
        Q_EMIT position_changed(now);
}

bool SciQLopStraightLine::read_only() const
{
    return m_target->read_only();
}

void SciQLopStraightLine::set_read_only(bool read_only)
{
    SCIQLOP_ON_OWNER_THREAD(set_read_only(read_only));
    if (m_target->read_only() == read_only)
        return;
    const QPointer<SciQLopStraightLine> alive(this);
    m_target->set_read_only(read_only);
    if (alive)
        Q_EMIT read_only_changed(read_only);
}

bool SciQLopStraightLine::movable() const
{
    return !read_only();
}

void SciQLopStraightLine::set_movable(bool movable)
{
    set_read_only(!movable);
}

QString SciQLopStraightLine::tool_tip() const
{
    return m_target->tool_tip();
}

void SciQLopStraightLine::set_tool_tip(const QString& tool_tip)
{
    SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
    m_target->set_tool_tip(tool_tip);
}

double SciQLopStraightLine::line_width() const
{
    return m_target->line_width();
}

void SciQLopStraightLine::set_line_width(double width)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_width(width));
    m_target->set_line_width(width);
}

Qt::PenStyle SciQLopStraightLine::line_style() const
{
    return m_target->line_style();
}

void SciQLopStraightLine::set_line_style(Qt::PenStyle style)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_style(style));
    m_target->set_line_style(style);
}

void SciQLopStraightLine::set_min_value(double min)
{
    SCIQLOP_ON_OWNER_THREAD(set_min_value(min));
    m_target->set_min_value(min);
}

void SciQLopStraightLine::clear_min_value()
{
    SCIQLOP_ON_OWNER_THREAD(clear_min_value());
    m_target->set_min_value(std::nullopt);
}

void SciQLopStraightLine::set_max_value(double max)
{
    SCIQLOP_ON_OWNER_THREAD(set_max_value(max));
    m_target->set_max_value(max);
}

void SciQLopStraightLine::clear_max_value()
{
    SCIQLOP_ON_OWNER_THREAD(clear_max_value());
    m_target->set_max_value(std::nullopt);
}

void SciQLopStraightLine::_set_visible(bool visible)
{
    m_target->set_visible(visible);
}

bool SciQLopStraightLine::_visible() const
{
    return m_target->visible();
}

void SciQLopStraightLine::_set_selected(bool selected)
{
    m_target->set_selected(selected);
}

bool SciQLopStraightLine::_selected() const
{
    return m_target->selected();
}

void SciQLopStraightLine::_set_color(const QColor& color)
{
    m_target->set_color(color);
}

QColor SciQLopStraightLine::_color() const
{
    return m_target->color();
}

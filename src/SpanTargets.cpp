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
#include "SciQLopPlots/Items/impl/SpanTargets.hpp"

#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/constants.hpp"
#include "SciQLopPlots/helpers.hpp"

#include <QSignalBlocker>
#include <utility>
#include <vector>

namespace impl
{
namespace
{
//! A NeoQCP span that SciQLopPlot's tooltip lookup finds: SpanBase is a SciQlopItemWithToolTip.
template <typename QCPSpan>
class SpanItem final : public QCPSpan, public SpanBase
{
public:
    explicit SpanItem(QCustomPlot* plot) : QCPSpan(plot), SpanBase(this)
    {
        this->setLayer(Constants::LayersNames::Spans);
        this->setSelectable(true);
    }
};

QCPAbstractSpanItem* make_item(QCustomPlot* plot, SpanKind kind)
{
    switch (kind)
    {
        case SpanKind::Vertical:
            return new SpanItem<QCPItemVSpan>(plot);
        case SpanKind::Horizontal:
            return new SpanItem<QCPItemHSpan>(plot);
        case SpanKind::Rectangular:
            return new SpanItem<QCPItemRSpan>(plot);
    }
    return nullptr;
}

//! The two edges that bound one axis of a span, and the pixel direction they move along.
struct Edges
{
    QCPItemPosition* lower;
    QCPItemPosition* upper;
    Qt::Orientation along;
};

Edges edges_of(QCPAbstractSpanItem* item, SpanAxis axis)
{
    if (auto* v = qobject_cast<QCPItemVSpan*>(item))
        return { v->lowerEdge, v->upperEdge, Qt::Horizontal };
    if (auto* h = qobject_cast<QCPItemHSpan*>(item))
        return { h->lowerEdge, h->upperEdge, Qt::Vertical };
    auto* r = qobject_cast<QCPItemRSpan*>(item);
    return axis == SpanAxis::Key ? Edges { r->leftEdge, r->rightEdge, Qt::Horizontal }
                                 : Edges { r->bottomEdge, r->topEdge, Qt::Vertical };
}

double along(const QPointF& point, Qt::Orientation orientation)
{
    return orientation == Qt::Horizontal ? point.x() : point.y();
}

// Pixels are measured from the plot area, not the widget: ptAbsolute's origin sits inside the
// axis margins, where clipToAxisRect() hides it.
QCPItemPosition::PositionType position_type(Coordinates coordinates)
{
    return coordinates == Coordinates::Pixels ? QCPItemPosition::ptAxisRectAbsolute
                                              : QCPItemPosition::ptPlotCoords;
}

void set_type(const Edges& edges, QCPItemPosition::PositionType type)
{
    for (auto* edge : { edges.lower, edges.upper })
    {
        if (edges.along == Qt::Horizontal)
            edge->setTypeX(type);
        else
            edge->setTypeY(type);
    }
}

double area_origin(const Edges& edges)
{
    const auto* rect = edges.lower->axisRect();
    return rect ? along(rect->topLeft(), edges.along) : 0.;
}

SciQLopPlotRange coords_of(const Edges& edges)
{
    return SciQLopPlotRange(along(edges.lower->coords(), edges.along),
                            along(edges.upper->coords(), edges.along));
}

// The resolved pixel positions are right in both modes; coordToPixel() would read pixel
// coordinates as data values.
SciQLopPlotRange pixels_of(const Edges& edges)
{
    const double origin = area_origin(edges);
    return SciQLopPlotRange(along(edges.lower->pixelPosition(), edges.along) - origin,
                            along(edges.upper->pixelPosition(), edges.along) - origin);
}

//! \a pixels (from the plot area) expressed in \a coordinates, through the edges' axis.
SciQLopPlotRange from_pixels(const Edges& edges, Coordinates coordinates,
                             const SciQLopPlotRange& pixels)
{
    if (coordinates == Coordinates::Pixels)
        return pixels;
    auto* axis = edges.along == Qt::Horizontal ? edges.lower->keyAxis() : edges.lower->valueAxis();
    if (!axis)
        return coords_of(edges);
    const double origin = area_origin(edges);
    return SciQLopPlotRange(axis->pixelToCoord(pixels.first + origin),
                            axis->pixelToCoord(pixels.second + origin));
}

//! Through NeoQCP's setters: they mark the GPU layer dirty, queue a replot and emit last.
void apply_range(QCPAbstractSpanItem* item, SpanAxis axis, const SciQLopPlotRange& range)
{
    const QCPRange r(range.first, range.second);
    if (auto* v = qobject_cast<QCPItemVSpan*>(item))
        v->setRange(r);
    else if (auto* h = qobject_cast<QCPItemHSpan*>(item))
        h->setRange(r);
    else if (auto* rect = qobject_cast<QCPItemRSpan*>(item))
    {
        if (axis == SpanAxis::Key)
            rect->setKeyRange(r);
        else
            rect->setValueRange(r);
    }
}
} // namespace

PlotSpanTarget::PlotSpanTarget(SciQLopSpan* span, SciQLopPlot* plot, SpanKind kind,
                               const SciQLopPlotRange& key_range,
                               const SciQLopPlotRange& value_range, Coordinates coordinates)
        : m_kind { kind }
        , m_item { make_item(plot->qcp_plot(), kind) }
        , m_base { dynamic_cast<SpanBase*>(m_item.data()) }
        , m_coordinates { coordinates }
{
    {
        const QSignalBlocker blocker(m_item.data());
        for (const auto axis : bounded_axes(kind))
        {
            // The type before the coordinates: setType keeps the pixel position it had.
            set_type(edges_of(m_item.data(), axis), position_type(coordinates));
            apply_range(m_item.data(), axis, axis == SpanAxis::Key ? key_range : value_range);
        }
    }
    // NeoQCP emits these for a drag and for our own setters alike: the one path to notify_range.
    const auto notify = [span](SpanAxis axis)
    {
        return [span, axis](const QCPRange& r)
        { span->notify_range(axis, SciQLopPlotRange(r.lower, r.upper)); };
    };
    QObject::connect(m_item.data(), &QCPAbstractItem::selectionChanged, span,
                     [span](bool selected) { span->_notify_selected(selected); });
    QObject::connect(m_item.data(), &QCPAbstractItem::deleteRequested, span,
                     &SciQLopSpan::delete_requested);
    if (auto* v = qobject_cast<QCPItemVSpan*>(m_item.data()))
        QObject::connect(v, &QCPItemVSpan::rangeChanged, span, notify(SpanAxis::Key));
    else if (auto* h = qobject_cast<QCPItemHSpan*>(m_item.data()))
        QObject::connect(h, &QCPItemHSpan::rangeChanged, span, notify(SpanAxis::Value));
    else if (auto* r = qobject_cast<QCPItemRSpan*>(m_item.data()))
    {
        QObject::connect(r, &QCPItemRSpan::keyRangeChanged, span, notify(SpanAxis::Key));
        QObject::connect(r, &QCPItemRSpan::valueRangeChanged, span, notify(SpanAxis::Value));
    }
}

PlotSpanTarget::~PlotSpanTarget()
{
    qcp_item_remove(m_item);
}

SciQLopPlotRange PlotSpanTarget::range(SpanAxis axis) const
{
    return m_item ? coords_of(edges_of(m_item.data(), axis)) : SciQLopPlotRange();
}

void PlotSpanTarget::set_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    if (m_item && this->range(axis) != range)
        apply_range(m_item.data(), axis, range);
}

SciQLopPlotRange PlotSpanTarget::pixel_range(SpanAxis axis) const
{
    return m_item ? pixels_of(edges_of(m_item.data(), axis)) : SciQLopPlotRange();
}

Coordinates PlotSpanTarget::coordinates() const
{
    return m_coordinates;
}

void PlotSpanTarget::set_coordinates(Coordinates coordinates)
{
    if (!m_item || coordinates == m_coordinates)
        return;
    const QSignalBlocker blocker(m_item.data());
    for (const auto axis : bounded_axes(m_kind))
    {
        const auto edges = edges_of(m_item.data(), axis);
        const auto pixels = pixels_of(edges);
        set_type(edges, position_type(coordinates));
        apply_range(m_item.data(), axis, from_pixels(edges, coordinates, pixels));
    }
    m_coordinates = coordinates;
}

bool PlotSpanTarget::read_only() const
{
    return !m_item || !m_item->movable();
}

void PlotSpanTarget::set_read_only(bool read_only)
{
    if (m_item)
        m_item->setMovable(!read_only);
}

QString PlotSpanTarget::tool_tip() const
{
    return m_item ? m_base->tooltip() : QString();
}

void PlotSpanTarget::set_tool_tip(const QString& tool_tip)
{
    if (m_item)
        m_base->setToolTip(tool_tip);
}

QColor PlotSpanTarget::borders_color() const
{
    return m_item ? m_base->borders_color() : QColor();
}

void PlotSpanTarget::set_borders_color(const QColor& color)
{
    if (!m_item)
        return;
    m_base->set_borders_color(color);
    m_base->replot();
}

double PlotSpanTarget::line_width() const
{
    return m_item ? m_base->line_width() : 0.;
}

void PlotSpanTarget::set_line_width(double width)
{
    if (!m_item)
        return;
    m_base->set_line_width(width);
    m_base->replot();
}

Qt::PenStyle PlotSpanTarget::line_style() const
{
    return m_item ? m_base->line_style() : Qt::SolidLine;
}

void PlotSpanTarget::set_line_style(Qt::PenStyle style)
{
    if (!m_item)
        return;
    m_base->set_line_style(style);
    m_base->replot();
}

QColor PlotSpanTarget::color() const
{
    return m_item ? m_base->color() : QColor();
}

void PlotSpanTarget::set_color(const QColor& color)
{
    if (!m_item)
        return;
    m_base->set_color(color);
    m_base->replot();
}

bool PlotSpanTarget::visible() const
{
    return m_item && m_item->visible();
}

void PlotSpanTarget::set_visible(bool visible)
{
    if (!m_item)
        return;
    m_item->setVisible(visible);
    m_base->replot();
}

bool PlotSpanTarget::selected() const
{
    return m_item && m_item->selected();
}

void PlotSpanTarget::set_selected(bool selected)
{
    if (!m_item)
        return;
    m_item->setSelected(selected);
    m_base->replot();
}
PanelSpanTarget::PanelSpanTarget(SciQLopSpan* span, SciQLopMultiPlotPanel* panel, SpanKind kind,
                                 SpanState state)
        : m_span { span }
        , m_kind { kind }
        , m_state { std::move(state) }
        , m_parts { span, [this](SciQLopPlot* plot) { return make_part(plot); } }
{
    m_parts.follow(panel);
}

SciQLopSpan* PanelSpanTarget::make_part(SciQLopPlot* plot)
{
    auto* part = new SciQLopSpan(plot, m_kind, m_state.key_range, m_state.value_range,
                                 m_state.color, m_state.read_only, m_state.visible,
                                 m_state.tool_tip, m_state.coordinates, m_span->id());
    part->set_borders_color(m_state.borders_color);
    part->set_line_width(m_state.line_width);
    part->set_line_style(m_state.line_style);
    part->set_selected(m_state.selected);
    follow(part);
    return part;
}

// A part's own edits (a drag, a click, the Delete key) flow up; the span writes them back out to
// every part, whose echoes writing() swallows.
void PanelSpanTarget::follow(SciQLopSpan* part)
{
    QObject::connect(part, &SciQLopSpan::key_range_changed, m_span,
                     [this](const SciQLopPlotRange& range)
                     {
                         if (!m_parts.writing())
                             set_range(SpanAxis::Key, range);
                     });
    QObject::connect(part, &SciQLopSpan::value_range_changed, m_span,
                     [this](const SciQLopPlotRange& range)
                     {
                         if (!m_parts.writing())
                             set_range(SpanAxis::Value, range);
                     });
    QObject::connect(part, &SciQLopDrawnObject::selection_changed, m_span,
                     [this](bool selected)
                     {
                         if (!m_parts.writing())
                             m_span->set_selected(selected);
                     });
    QObject::connect(part, &SciQLopSpan::delete_requested, m_span, &SciQLopSpan::delete_requested);
}

bool PanelSpanTarget::store_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    (axis == SpanAxis::Key ? m_state.key_range : m_state.value_range) = range;
    return m_parts.write(
        [axis, range](SciQLopSpan* part)
        {
            if (axis == SpanAxis::Key)
                part->set_key_range(range);
            else
                part->set_value_range(range);
        });
}

template <typename T, typename Setter>
void PanelSpanTarget::write(T& field, const T& value, Setter setter)
{
    field = value;
    (void)m_parts.write([value, setter](SciQLopSpan* part) { (part->*setter)(value); });
}

SciQLopPlotRange PanelSpanTarget::range(SpanAxis axis) const
{
    return axis == SpanAxis::Key ? m_state.key_range : m_state.value_range;
}

void PanelSpanTarget::set_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    if (this->range(axis) == range)
        return;
    const SciQLopPlotRange stored = range; // \a range may live in a part a slot deletes
    if (store_range(axis, stored))
        m_span->notify_range(axis, stored);
}

// simplify: the first plot's pixels stand for the panel's; a per-plot answer needs a plot argument.
SciQLopPlotRange PanelSpanTarget::pixel_range(SpanAxis axis) const
{
    auto* part = m_parts.first();
    if (!part)
        return SciQLopPlotRange();
    return part->m_target->pixel_range(axis);
}

Coordinates PanelSpanTarget::coordinates() const
{
    return m_state.coordinates;
}

// simplify: each part converts on its own plot, then the first part's numbers become the
// panel's and every part takes them, so plots of different sizes share one pixel range; with no
// plot yet the numbers stay as they are. A range per plot would lift both limits.
void PanelSpanTarget::set_coordinates(Coordinates coordinates)
{
    m_state.coordinates = coordinates;
    if (!m_parts.write([coordinates](SciQLopSpan* part) { part->set_coordinates(coordinates); }))
        return;
    auto* first = m_parts.first();
    if (!first)
        return;
    std::vector<std::pair<SpanAxis, SciQLopPlotRange>> converted;
    for (const auto axis : bounded_axes(m_kind))
        converted.emplace_back(axis, first->m_target->range(axis));
    for (const auto& [axis, range] : converted)
        if (!store_range(axis, range))
            return;
}

bool PanelSpanTarget::read_only() const
{
    return m_state.read_only;
}

void PanelSpanTarget::set_read_only(bool read_only)
{
    write(m_state.read_only, read_only, &SciQLopSpan::set_read_only);
}

QString PanelSpanTarget::tool_tip() const
{
    return m_state.tool_tip;
}

void PanelSpanTarget::set_tool_tip(const QString& tool_tip)
{
    write(m_state.tool_tip, tool_tip, &SciQLopSpan::set_tool_tip);
}

QColor PanelSpanTarget::color() const
{
    return m_state.color;
}

void PanelSpanTarget::set_color(const QColor& color)
{
    write(m_state.color, color, &SciQLopDrawnObject::set_color);
}

QColor PanelSpanTarget::borders_color() const
{
    return m_state.borders_color;
}

void PanelSpanTarget::set_borders_color(const QColor& color)
{
    write(m_state.borders_color, color, &SciQLopSpan::set_borders_color);
}

double PanelSpanTarget::line_width() const
{
    return m_state.line_width;
}

void PanelSpanTarget::set_line_width(double width)
{
    write(m_state.line_width, width, &SciQLopSpan::set_line_width);
}

Qt::PenStyle PanelSpanTarget::line_style() const
{
    return m_state.line_style;
}

void PanelSpanTarget::set_line_style(Qt::PenStyle style)
{
    write(m_state.line_style, style, &SciQLopSpan::set_line_style);
}

bool PanelSpanTarget::visible() const
{
    return m_state.visible;
}

void PanelSpanTarget::set_visible(bool visible)
{
    write(m_state.visible, visible, &SciQLopDrawnObject::set_visible);
}

bool PanelSpanTarget::selected() const
{
    return m_state.selected;
}

void PanelSpanTarget::set_selected(bool selected)
{
    write(m_state.selected, selected, &SciQLopDrawnObject::set_selected);
}
} // namespace impl

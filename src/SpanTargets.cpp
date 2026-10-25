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
        this->setBorderPen(QPen(QBrush(Qt::black), 3));
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

SciQLopPlotRange coords_of(const Edges& edges)
{
    return SciQLopPlotRange(along(edges.lower->coords(), edges.along),
                            along(edges.upper->coords(), edges.along));
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

Coordinates PlotSpanTarget::coordinates() const
{
    return m_coordinates;
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
} // namespace impl

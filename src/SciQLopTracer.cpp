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
#include "SciQLopPlots/Items/SciQLopTracer.hpp"
#include <fmt/chrono.h>
#include <fmt/core.h>

namespace _impl
{
int _nanoseconds(const double epoch)
{
    return static_cast<int>((epoch - static_cast<int64_t>(epoch)) * 1e9);
}

inline std::string _render_value(const double value)
{
    return fmt::format("{:.4g}", value);
}

std::string _render_value(const double value, const QCPAxis* const axis)
{
    if (auto datetime_ticker = axis->ticker().dynamicCast<const QCPAxisTickerDateTime>();
        !datetime_ticker.isNull())
    {
        const auto duration = axis->range().size();
        const auto datetime = std::chrono::system_clock::from_time_t(value)
            + std::chrono::nanoseconds(_nanoseconds(value));
        if (duration > 60 * 60 * 24)
            return fmt::format("{:%Y-%m-%d %H:%M}", datetime);
        if (duration > 60 * 60)
            return fmt::format("{:%H:%M:%S}",
                               std::chrono::floor<std::chrono::milliseconds>(datetime));
        return fmt::format("{:%M:%S}", std::chrono::floor<std::chrono::microseconds>(datetime));
    }
    return _render_value(value);
}

PlotQuadrant _quadrant(const QPointF& pos, const QCPAxisRect* axisRect)
{
    const auto rect = axisRect->rect();
    if (pos.x() < rect.center().x())
    {
        if (pos.y() < rect.center().y())
            return PlotQuadrant::TopLeft;
        return PlotQuadrant::BottomLeft;
    }
    if (pos.y() < rect.center().y())
        return PlotQuadrant::TopRight;
    return PlotQuadrant::BottomRight;
}

void _update_tooltip_alignment(QCPItemRichText* tooltip, const PlotQuadrant quadrant)
{
    switch (quadrant)
    {
        case PlotQuadrant::TopLeft:
            tooltip->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
            break;
        case PlotQuadrant::TopRight:
            tooltip->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
            break;
        case PlotQuadrant::BottomLeft:
            tooltip->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
            break;
        case PlotQuadrant::BottomRight:
            tooltip->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
            break;
        case PlotQuadrant::None:
            break;
    }
}

} // namespace _impl

TracerWithToolTip::TracerWithToolTip(QCustomPlot* parent)
        : m_tooltip(new QCPItemRichText(parent)), m_tracer(new SciQLopTracer(parent))
{
    m_tooltip->setClipToAxisRect(false);
    m_tooltip->setPadding(QMargins(5, 5, 5, 5));
    m_tooltip->setBrush(QBrush(QColor(0xee, 0xf2, 0xf7, 200)));
    m_tooltip->setPen(QPen(QBrush(QColor(0, 0, 0, 200)), 1., Qt::DashLine));

    m_tooltip->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_tooltip->setTextAlignment(Qt::AlignLeft);
    m_tooltip->setLayer("overlay");
    m_tooltip->setVisible(false);
    m_tracer->setLayer("overlay");
    m_tracer->setStyle(QCPItemTracer::TracerStyle::tsCircle);
}

TracerWithToolTip::~TracerWithToolTip() { }

void TracerWithToolTip::update_position(const QPointF& pos, bool do_replot)
{
    if (m_tracer->plotable() == nullptr)
        return;
    if (!visible())
        set_visible(true);
    m_tracer->updatePosition(pos);
    const double new_x = m_tracer->key();
    const double new_y = m_tracer->value();
    const double new_data = m_tracer->data();
    if (new_x == m_x && new_y == m_y && new_data == m_data)
        return;
    m_x = new_x;
    m_y = new_y;
    m_data = new_data;
    _impl::_update_tooltip_alignment(m_tooltip, m_tracer->quadrant());
    m_tooltip->position->setPixelPosition(m_tracer->position->pixelPosition());
    auto keyAxis = m_tracer->plotable()->keyAxis();
    auto valueAxis = m_tracer->plotable()->valueAxis();
    if (!std::isnan(m_data))
        m_tooltip->setHtml(QString::fromStdString(fmt::format(
            "x: <b>{}</b><br>y: <b>{}</b><br>z: <b>{}</b>", _impl::_render_value(m_x, keyAxis),
            _impl::_render_value(m_y, valueAxis), _impl::_render_value(m_data))));
    else
        m_tooltip->setHtml(QString::fromStdString(fmt::format("x: <b>{}</b><br>y: <b>{}</b>",
                                       _impl::_render_value(m_x, keyAxis),
                                       _impl::_render_value(m_y, valueAxis))));
    if (do_replot)
        this->replot();
}

QPen SciQLopTracer::mainPen() const
{
    return mSelected ? m_SelectedPen : m_Pen;
}

QBrush SciQLopTracer::mainBrush() const
{
    return mSelected ? m_SelectedBrush : m_Brush;
}

SciQLopTracer::SciQLopTracer(QCustomPlot* parentPlot)
        : QCPAbstractItem(parentPlot)
        , m_Size(6)
        , m_Style(TracerStyle::tsCrosshair)
        , position(createPosition(QLatin1String("position")))
{
    setBrush(Qt::NoBrush);
    setSelectedBrush(Qt::NoBrush);
    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
    position->setType(QCPItemPosition::ptPlotCoords);
}

void SciQLopTracer::setPlotable(QCPAbstractPlottable* plottable)
{
    m_locator.setPlottable(plottable);
    if (plottable)
    {
        position->setAxes(plottable->keyAxis(), plottable->valueAxis());
        if (auto* graph = dynamic_cast<QCPGraph*>(plottable))
            setPen(graph->pen());
        else if (auto* curve = dynamic_cast<QCPCurve*>(plottable))
            setPen(curve->pen());
        else
            setPen(QPen(Qt::black));
    }
    else
    {
        setPen(QPen(Qt::black));
    }
}

void SciQLopTracer::updatePosition(const QPointF& pos)
{
    if (m_locator.locate(pos))
    {
        position->setCoords(m_locator.key(), m_locator.value());
        if (auto* plottable = m_locator.plottable())
            m_quadrant = _impl::_quadrant(pos, plottable->valueAxis()->axisRect());
    }
}

double SciQLopTracer::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    Q_UNUSED(details)
    if (onlySelectable && !mSelectable)
        return -1;

    QPointF center(position->pixelPosition());
    if (center.x() < 0 || center.y() < 0)
        return -1;
    double w = m_Size / 2.0;
    QRect clip = clipRect();
    switch (m_Style)
    {
        case TracerStyle::tsNone:
            return -1;
        case TracerStyle::tsPlus:
        {
            if (clipRect().intersects(
                    QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
                return qSqrt(qMin(QCPVector2D(pos).distanceSquaredToLine(center + QPointF(-w, 0),
                                                                         center + QPointF(w, 0)),
                                  QCPVector2D(pos).distanceSquaredToLine(center + QPointF(0, -w),
                                                                         center + QPointF(0, w))));
            break;
        }
        case TracerStyle::tsCrosshair:
        {
            return qSqrt(qMin(
                QCPVector2D(pos).distanceSquaredToLine(QCPVector2D(clip.left(), center.y()),
                                                       QCPVector2D(clip.right(), center.y())),
                QCPVector2D(pos).distanceSquaredToLine(QCPVector2D(center.x(), clip.top()),
                                                       QCPVector2D(center.x(), clip.bottom()))));
        }
        case TracerStyle::tsCircle:
        {
            if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
            {
                // distance to border:
                double centerDist = QCPVector2D(center - pos).length();
                double circleLine = w;
                double result = qAbs(centerDist - circleLine);
                // filled ellipse, allow click inside to count as hit:
                if (result > mParentPlot->selectionTolerance() * 0.99
                    && m_Brush.style() != Qt::NoBrush && m_Brush.color().alpha() != 0)
                {
                    if (centerDist <= circleLine)
                        result = mParentPlot->selectionTolerance() * 0.99;
                }
                return result;
            }
            break;
        }
        case TracerStyle::tsSquare:
        {
            if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
            {
                QRectF rect = QRectF(center - QPointF(w, w), center + QPointF(w, w));
                bool filledRect = m_Brush.style() != Qt::NoBrush && m_Brush.color().alpha() != 0;
                return rectDistance(rect, pos, filledRect);
            }
            break;
        }
    }
    return -1;
}

QCPAxis* SciQLopTracer::keyAxis() const
{
    return position->keyAxis();
}

QCPAxis* SciQLopTracer::valueAxis() const
{
    return position->valueAxis();
}

void SciQLopTracer::draw(QCPPainter* painter)
{
    if (m_Style == TracerStyle::tsNone)
        return;

    painter->setPen(mainPen());
    painter->setBrush(mainBrush());
    QPointF center(position->pixelPosition());
    double w = m_Size / 2.0;
    QRect clip = clipRect();
    if (w>0 && center.x()>=0 && center.y()>=0)
    {
        switch (m_Style)
        {
            case TracerStyle::tsNone:
                return;
            case TracerStyle::tsPlus:
            {
                if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
                {
                    painter->drawLine(QLineF(center + QPointF(-w, 0), center + QPointF(w, 0)));
                    painter->drawLine(QLineF(center + QPointF(0, -w), center + QPointF(0, w)));
                }
                break;
            }
            case TracerStyle::tsCrosshair:
            {
                if (center.y() > clip.top() && center.y() < clip.bottom())
                    painter->drawLine(QLineF(clip.left(), center.y(), clip.right(), center.y()));
                if (center.x() > clip.left() && center.x() < clip.right())
                    painter->drawLine(QLineF(center.x(), clip.top(), center.x(), clip.bottom()));
                break;
            }
            case TracerStyle::tsCircle:
            {
                if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
                    painter->drawEllipse(center, w, w);
                break;
            }
            case TracerStyle::tsSquare:
            {
                if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
                    painter->drawRect(QRectF(center - QPointF(w, w), center + QPointF(w, w)));
                break;
            }
        }
    }
}


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
int _microseconds(const double epoch)
{
    return static_cast<int>((epoch * 1e3 - static_cast<int>(epoch * 1e3)) * 1e3);
}

int _nanoseconds(const double epoch)
{
    return static_cast<int>((epoch - static_cast<int>(epoch)) * 1e9);
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

std::optional<std::tuple<double, double>> _nearest_data_point(const QPointF& pos,
                                                              const QCPGraph* graph)
{
    QCPGraphDataContainer::const_iterator it = graph->data()->constEnd();
    QVariant details;
    if (graph->selectTest(pos, false, &details))
    {
        QCPDataSelection dataPoints = details.value<QCPDataSelection>();
        if (dataPoints.dataPointCount() > 0)
        {
            it = graph->data()->at(dataPoints.dataRange().begin());
            return std::make_tuple(it->key, it->value);
        }
    }
    return std::nullopt;
}

std::optional<std::tuple<double, double>> _nearest_data_point(const QPointF& pos,
                                                              const QCPCurve* curve)
{
    auto it = curve->data()->constEnd();
    QVariant details;
    if (curve->selectTest(pos, false, &details))
    {
        QCPDataSelection dataPoints = details.value<QCPDataSelection>();
        if (dataPoints.dataPointCount() > 0)
        {
            it = curve->data()->at(dataPoints.dataRange().begin());
            return std::make_tuple(it->key, it->value);
        }
    }
    return std::nullopt;
}

double _discretize(const double value, const double lower, const double upper, const int count)
{
    const auto q = (upper - lower) / count;
    return (floor((value - lower) / q) + 0.5) * q + lower;
}

double _nearest_axis_value(const double value, const QCPRange& range,
                           const QCPAxis::ScaleType scale_type, const int count)
{
    if (scale_type == QCPAxis::stLinear)
    {
        return _discretize(value, range.lower, range.upper, count);
    }
    else // log scale
    {
        return std::pow(10.,
                        _discretize(log10(value), log10(range.lower), log10(range.upper), count));
    }
}

int _log_scale_index(const double value, const QCPRange& range, const int count)
{
    return static_cast<int>(count * (log10(value) - log10(range.lower))
                            / (log10(range.upper) - log10(range.lower)));
}

std::optional<std::tuple<double, double, double>> _nearest_data_point(const QPointF& pos,
                                                                      const QCPColorMap* colormap)
{

    const auto key = _nearest_axis_value(
        colormap->keyAxis()->pixelToCoord(pos.x()), colormap->data()->keyRange(),
        colormap->keyAxis()->scaleType(), colormap->data()->keySize());
    const auto value = _nearest_axis_value(
        colormap->valueAxis()->pixelToCoord(pos.y()), colormap->data()->valueRange(),
        colormap->valueAxis()->scaleType(), colormap->data()->valueSize());

    int keyIndex, valueIndex;
    if (colormap->keyAxis()->scaleType() == QCPAxis::stLogarithmic)
    {
        keyIndex = _log_scale_index(key, colormap->data()->keyRange(), colormap->data()->keySize());
    }
    else
    {
        colormap->data()->coordToCell(key, value, &keyIndex, nullptr);
    }
    if (colormap->valueAxis()->scaleType() == QCPAxis::stLogarithmic)
    {
        valueIndex = _log_scale_index(value, colormap->data()->valueRange(),
                                      colormap->data()->valueSize());
    }
    else
    {
        colormap->data()->coordToCell(key, value, nullptr, &valueIndex);
    }
    const auto data = colormap->data()->cell(keyIndex, valueIndex);
    return std::make_tuple(key, value, data);
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
    m_tracer->setStyle(QCPItemTracer::TracerStyle::tsCircle);
}

TracerWithToolTip::~TracerWithToolTip() { }

void TracerWithToolTip::update_position(const QPointF& pos, bool replot)
{
    if (m_tracer->plotable() == nullptr)
        return;
    if (!visible())
        set_visible(true);
    m_tracer->updatePosition(pos);
    m_x = m_tracer->key();
    m_y = m_tracer->value();
    m_data = m_tracer->data();
    _impl::_update_tooltip_alignment(m_tooltip, m_tracer->quadrant());
    m_tooltip->position->setPixelPosition(m_tracer->position->pixelPosition());
    auto keyAxis = m_tracer->plotable()->keyAxis();
    auto valueAxis = m_tracer->plotable()->valueAxis();
    if (!std::isnan(m_data))
        m_tooltip->setHtml(fmt::format(
            "x: <b>{}</b><br>y: <b>{}</b><br>z: <b>{}</b>", _impl::_render_value(m_x, keyAxis),
            _impl::_render_value(m_y, valueAxis), _impl::_render_value(m_data)));
    else
        m_tooltip->setHtml(fmt::format("x: <b>{}</b><br>y: <b>{}</b>",
                                       _impl::_render_value(m_x, keyAxis),
                                       _impl::_render_value(m_y, valueAxis)));
    if (replot)
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
    m_locator.set_plottable(plottable);
    if (plottable)
    {
        position->setAxes(plottable->keyAxis(), plottable->valueAxis());
    }
    switch (m_locator.plotable_type())
    {
        case PlotableType::Graph:
            setPen(dynamic_cast<QCPGraph*>(plottable)->pen());
            break;
        case PlotableType::Curve:
            setPen(dynamic_cast<QCPCurve*>(plottable)->pen());
            break;
        default:
            setPen(QPen(Qt::black));
            break;
    }
}

void SciQLopTracer::updatePosition(const QPointF& pos)
{
    m_locator.setPosition(pos);
    position->setCoords(m_locator.key(), m_locator.value());
}

double SciQLopTracer::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    Q_UNUSED(details)
    if (onlySelectable && !mSelectable)
        return -1;

    QPointF center(position->pixelPosition());
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

void QCPAbstractPlottableDataLocator::set_plottable(QCPAbstractPlottable* plottable) noexcept
{
    m_plottable = plottable;
    if (plottable == nullptr)
        m_plotableType = PlotableType::None;
    else if (qobject_cast<QCPGraph*>(plottable))
        m_plotableType = PlotableType::Graph;
    else if (qobject_cast<QCPCurve*>(plottable))
        m_plotableType = PlotableType::Curve;
    else if (qobject_cast<QCPColorMap*>(plottable))
        m_plotableType = PlotableType::ColorMap;
    else
        m_plotableType = PlotableType::None;
    m_key = std::nan("");
    m_value = std::nan("");
    m_data = std::nan("");
}

void QCPAbstractPlottableDataLocator::setPosition(const QPointF& pos)
{
    switch (m_plotableType)
    {
        case PlotableType::Graph:
            std::tie(m_key, m_value, m_data)
                = locate_data_graph(pos, dynamic_cast<QCPGraph*>(m_plottable));
            m_quadrant = _impl::_quadrant(
                pos, dynamic_cast<QCPGraph*>(m_plottable)->valueAxis()->axisRect());
            break;
        case PlotableType::Curve:
            std::tie(m_key, m_value, m_data)
                = locate_data_curve(pos, dynamic_cast<QCPCurve*>(m_plottable));
            m_quadrant = _impl::_quadrant(
                pos, dynamic_cast<QCPCurve*>(m_plottable)->valueAxis()->axisRect());
            break;
        case PlotableType::ColorMap:
            std::tie(m_key, m_value, m_data)
                = locate_data_colormap(pos, dynamic_cast<QCPColorMap*>(m_plottable));
            m_quadrant = _impl::_quadrant(
                pos, dynamic_cast<QCPColorMap*>(m_plottable)->valueAxis()->axisRect());
            break;
        case PlotableType::None:
            break;
        default:
            break;
    }
}

std::tuple<double, double, double>
QCPAbstractPlottableDataLocator::locate_data_graph(const QPointF& pos, QCPGraph* graph)
{
    if (auto data = _impl::_nearest_data_point(pos, graph); data.has_value())
    {
        return std::make_tuple(std::get<0>(data.value()), std::get<1>(data.value()), std::nan(""));
    }
    return std::make_tuple(std::nan(""), std::nan(""), std::nan(""));
}

std::tuple<double, double, double>
QCPAbstractPlottableDataLocator::locate_data_curve(const QPointF& pos, QCPCurve* curve)
{
    if (auto data = _impl::_nearest_data_point(pos, curve); data.has_value())
    {
        return std::make_tuple(std::get<0>(data.value()), std::get<1>(data.value()), std::nan(""));
    }
    return std::make_tuple(std::nan(""), std::nan(""), std::nan(""));
}

std::tuple<double, double, double>
QCPAbstractPlottableDataLocator::locate_data_colormap(const QPointF& pos, QCPColorMap* colormap)
{
    return *_impl::_nearest_data_point(pos, colormap);
}

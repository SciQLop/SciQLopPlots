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
#include "SciQLopPlots/SciQLopTracer.hpp"
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
            return fmt::format(
                "{:%H:%M:%S}", std::chrono::floor<std::chrono::milliseconds>(datetime));
        return fmt::format("{:%M:%S}", std::chrono::floor<std::chrono::microseconds>(datetime));
    }
    return fmt::format("{:.3f}", value);
}

std::optional<std::tuple<double, double>> _nearest_data_point(
    const QPointF& pos, const QCPGraph* graph)
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

enum class PlotQuadrant
{
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

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

void _update_tooltip_alignment(QCPItemRichText* tooltip, const QPointF& pos, const QCPGraph* graph)
{
    const auto quadrant = _quadrant(pos, graph->valueAxis()->axisRect());
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
    }
}

} // namespace _impl


TracerWithToolTip::TracerWithToolTip(QCustomPlot* parent)
        : m_tooltip(new QCPItemRichText(parent)), m_tracer(new QCPItemTracer(parent))
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

TracerWithToolTip::~TracerWithToolTip()
{
    delete m_tooltip;
    delete m_tracer;
}

void TracerWithToolTip::update_position(const QPointF& pos, bool replot)
{
    if (m_tracer->graph() == nullptr)
        return;
    if (!visible())
        set_visible(true);
    const auto graph = m_tracer->graph();
    std::tie(m_x, m_y) = _impl::_nearest_data_point(pos, graph).value_or(std::make_tuple(0, 0));
    m_tracer->setGraphKey(m_x);
    _impl::_update_tooltip_alignment(m_tooltip, pos, graph);
    m_tooltip->position->setCoords(m_x, m_y);
    m_tooltip->setHtml(
        fmt::format("x: <b>{}</b><br>y: <b>{}</b>", _impl::_render_value(m_x, graph->keyAxis()),
            _impl::_render_value(m_y, graph->valueAxis())));
    if (replot)
        this->replot();
}

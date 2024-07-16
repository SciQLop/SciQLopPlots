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


namespace _impl
{
QString _render_value(const double value, const QCPAxis* const axis)
{
    if (auto datetime_ticker = axis->ticker().dynamicCast<const QCPAxisTickerDateTime>();
        !datetime_ticker.isNull())
    {
        return QCPAxisTickerDateTime::keyToDateTime(value).toString(
            datetime_ticker->dateTimeFormat());
    }
    return QString::number(value);
}

std::optional<std::tuple<double, double>> _nearest_data_point(const QPointF& pos, QCPGraph* graph)
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
} // namespace _impl


TracerWithToolTip::TracerWithToolTip(QCustomPlot* parent)
        : m_tooltip(new QCPItemRichText(parent)), m_tracer(new QCPItemTracer(parent))
{
    m_tooltip->setClipToAxisRect(false);
    m_tooltip->setPadding(QMargins(5, 5, 5, 5));
    m_tooltip->setBrush(QBrush(QColor(0xeef2f7)));
    m_tooltip->setPen(QPen(QColor(0, 0, 0, 200)));
    m_tooltip->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_tooltip->setTextAlignment(Qt::AlignLeft);
    m_tooltip->setLayer(this->m_tracer->layer());
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
    const auto [x, y]
        = _impl::_nearest_data_point(pos, m_tracer->graph()).value_or(std::make_tuple(0, 0));
    m_x = x;
    m_y = y;
    m_tracer->setGraphKey(x);
    m_tooltip->position->setCoords(x, y);
    const auto graph = m_tracer->graph();
    m_tooltip->setHtml(QString("x: <b>%1</b><hr>y: <b>%2</b>")
                           .arg(_impl::_render_value(x, graph->keyAxis()),
                               _impl::_render_value(y, graph->valueAxis())));
    if (replot)
        this->replot();
}

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
#include "SciQLopPlots/Plotables/SciQLopTimeColoredCurve.hpp"
#include <algorithm>
#include <cmath>

QColor SciQLopTimeColoredCurve::color_for_normalized(double f) const
{
    f = std::clamp(f, 0.0, 1.0);
    return QColor(
        m_gradient_start.red() + f * (m_gradient_end.red() - m_gradient_start.red()),
        m_gradient_start.green() + f * (m_gradient_end.green() - m_gradient_start.green()),
        m_gradient_start.blue() + f * (m_gradient_end.blue() - m_gradient_start.blue()));
}

void SciQLopTimeColoredCurve::set_time_values(const QVector<double>& times)
{
    m_time_values = times;
    if (!times.isEmpty())
    {
        const auto [min_it, max_it] = std::minmax_element(times.begin(), times.end());
        m_t_min = *min_it;
        m_t_max = *max_it;
    }
}

void SciQLopTimeColoredCurve::draw(QCPPainter* painter)
{
    const double t_range = m_t_max - m_t_min;
    if (!m_time_color_enabled || m_time_values.isEmpty() || t_range <= 0.0)
    {
        QCPCurve::draw(painter);
        return;
    }

    if (mDataContainer->isEmpty())
        return;

    QCPAxis* keyAxis = mKeyAxis.data();
    QCPAxis* valueAxis = mValueAxis.data();
    if (!keyAxis || !valueAxis)
        return;

    const QRectF clip_rect = keyAxis->axisRect()->rect().adjusted(-10, -10, 10, 10);
    applyDefaultAntialiasingHint(painter);
    QPen seg_pen = mPen;

    const int n_times = m_time_values.size();

    auto it = mDataContainer->constBegin();
    auto end = mDataContainer->constEnd();

    QPointF prev_px(keyAxis->coordToPixel(it->key), valueAxis->coordToPixel(it->value));
    ++it;

    for (; it != end; ++it)
    {
        QPointF cur_px(keyAxis->coordToPixel(it->key), valueAxis->coordToPixel(it->value));
        if (clip_rect.contains(prev_px) || clip_rect.contains(cur_px))
        {
            const int idx = static_cast<int>(it->t);
            if (idx >= 0 && idx < n_times)
            {
                const double f = (m_time_values[idx] - m_t_min) / t_range;
                seg_pen.setColor(color_for_normalized(f));
            }
            painter->setPen(seg_pen);
            painter->drawLine(prev_px, cur_px);
        }
        prev_px = cur_px;
    }
}

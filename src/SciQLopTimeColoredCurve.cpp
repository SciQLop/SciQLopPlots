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
    set_color_values(times);
}

void SciQLopTimeColoredCurve::set_color_values(const QVector<double>& values)
{
    m_color_values = values;
    if (!values.isEmpty())
    {
        const auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
        m_c_min = *min_it;
        m_c_max = *max_it;
    }
}

std::optional<QPointF> SciQLopTimeColoredCurve::position_at_time(double t) const
{
    if (m_time_values.isEmpty() || mDataContainer->isEmpty())
        return std::nullopt;

    auto it = std::lower_bound(m_time_values.begin(), m_time_values.end(), t);
    int idx;
    if (it == m_time_values.end())
        idx = m_time_values.size() - 1;
    else if (it == m_time_values.begin())
        idx = 0;
    else
    {
        int hi = static_cast<int>(it - m_time_values.begin());
        int lo = hi - 1;
        idx = (t - m_time_values[lo] <= m_time_values[hi] - t) ? lo : hi;
    }

    if (idx < mDataContainer->size())
    {
        const auto& d = *(mDataContainer->constBegin() + idx);
        return QPointF(d.key, d.value);
    }
    return std::nullopt;
}

void SciQLopTimeColoredCurve::draw(QCPPainter* painter)
{
    const double c_range = m_c_max - m_c_min;
    if (!m_time_color_enabled || m_color_values.isEmpty() || c_range <= 0.0)
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

    const int n_colors = m_color_values.size();
    constexpr int color_buckets = 256;
    const double inv_c_range = 1.0 / c_range;

    auto it = mDataContainer->constBegin();
    auto end = mDataContainer->constEnd();

    QPointF prev_px(keyAxis->coordToPixel(it->key), valueAxis->coordToPixel(it->value));
    int prev_bucket = -1;
    QVector<QPointF> batch;
    batch.reserve(256);
    batch.append(prev_px);
    ++it;

    auto flush = [&](int bucket) {
        if (batch.size() >= 2)
        {
            const double f = std::clamp(static_cast<double>(bucket) / color_buckets, 0.0, 1.0);
            seg_pen.setColor(color_for_normalized(f));
            painter->setPen(seg_pen);
            painter->drawPolyline(batch.data(), batch.size());
        }
        batch.clear();
    };

    for (; it != end; ++it)
    {
        QPointF cur_px(keyAxis->coordToPixel(it->key), valueAxis->coordToPixel(it->value));

        const double dx = cur_px.x() - prev_px.x();
        const double dy = cur_px.y() - prev_px.y();
        if (dx * dx + dy * dy < 0.25)
            continue;

        int bucket = 0;
        const int idx = static_cast<int>(it->t);
        if (idx >= 0 && idx < n_colors)
            bucket = static_cast<int>((m_color_values[idx] - m_c_min) * inv_c_range * color_buckets);

        const bool visible = clip_rect.contains(prev_px) || clip_rect.contains(cur_px);

        if (visible && bucket == prev_bucket)
        {
            batch.append(cur_px);
        }
        else
        {
            flush(prev_bucket);
            batch.append(prev_px);
            if (visible)
                batch.append(cur_px);
        }

        prev_bucket = bucket;
        prev_px = cur_px;
    }
    flush(prev_bucket);
}

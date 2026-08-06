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
#include <array>
#include <cmath>

namespace
{
QCPColorGradient two_stop_gradient(const QColor& start, const QColor& end)
{
    QCPColorGradient gradient;
    gradient.clearColorStops();
    gradient.setColorStopAt(0.0, start);
    gradient.setColorStopAt(1.0, end);
    return gradient;
}
}

SciQLopTimeColoredCurve::SciQLopTimeColoredCurve(QCPAxis* keyAxis, QCPAxis* valueAxis)
        : QCPCurve(keyAxis, valueAxis)
        , m_gradient { two_stop_gradient(QColor(0, 0, 255), QColor(255, 0, 0)) }
{
    rebuild_lut();
}

void SciQLopTimeColoredCurve::set_gradient_colors(const QColor& start, const QColor& end)
{
    m_gradient = two_stop_gradient(start, end);
    rebuild_lut();
}

void SciQLopTimeColoredCurve::set_color_gradient(const QCPColorGradient& gradient)
{
    m_gradient = gradient;
    rebuild_lut();
}

void SciQLopTimeColoredCurve::rebuild_lut()
{
    // QCPColorGradient only exposes bulk colorization, so bake the whole ramp
    // once per gradient change and index into it while painting.
    std::array<double, color_buckets> positions {};
    for (int i = 0; i < color_buckets; ++i)
        positions[i] = static_cast<double>(i) / (color_buckets - 1);
    m_gradient.colorize(positions.data(), QCPRange(0.0, 1.0), m_lut.data(), color_buckets, 1,
                        false);
}

int SciQLopTimeColoredCurve::bucket_at(int index) const noexcept
{
    if (index < 0 || index >= m_color_values.size())
        return 0;
    const double f = (m_color_values[index] - m_c_min) / (m_c_max - m_c_min);
    return std::clamp(static_cast<int>(f * color_buckets), 0, color_buckets - 1);
}

QColor SciQLopTimeColoredCurve::color_for_bucket(int bucket) const
{
    return QColor::fromRgb(m_lut[std::clamp(bucket, 0, color_buckets - 1)]);
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
    if (!colouring_active())
    {
        QCPCurve::draw(painter);
        return;
    }

    if (mDataContainer->isEmpty() || !mKeyAxis || !mValueAxis)
        return;

    // draw() bypasses QCPCurve's own culling, so cull against a slightly grown
    // axis rect here.
    const QRectF clip_rect = mKeyAxis.data()->axisRect()->rect().adjusted(-10, -10, 10, 10);

    if (mLineStyle != lsNone)
        draw_colored_line(painter, clip_rect);
    if (!mScatterStyle.isNone())
        draw_colored_scatters(painter, clip_rect);
}

void SciQLopTimeColoredCurve::draw_colored_line(QCPPainter* painter, const QRectF& clip_rect)
{
    QCPAxis* keyAxis = mKeyAxis.data();
    QCPAxis* valueAxis = mValueAxis.data();

    applyDefaultAntialiasingHint(painter);
    QPen seg_pen = mPen;

    auto it = mDataContainer->constBegin();
    const auto end = mDataContainer->constEnd();

    QPointF prev_px(keyAxis->coordToPixel(it->key), valueAxis->coordToPixel(it->value));
    int prev_bucket = -1;
    QVector<QPointF> batch;
    batch.reserve(256);
    batch.append(prev_px);
    ++it;

    const auto flush = [&](int bucket)
    {
        if (batch.size() >= 2)
        {
            seg_pen.setColor(color_for_bucket(bucket));
            painter->setPen(seg_pen);
            painter->drawPolyline(batch.data(), batch.size());
        }
        batch.clear();
    };

    for (; it != end; ++it)
    {
        QPointF cur_px(keyAxis->coordToPixel(it->key), valueAxis->coordToPixel(it->value));

        // Sub-pixel steps add nothing but painter calls.
        const double dx = cur_px.x() - prev_px.x();
        const double dy = cur_px.y() - prev_px.y();
        if (dx * dx + dy * dy < 0.25)
            continue;

        const int bucket = bucket_at(static_cast<int>(it->t));
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

void SciQLopTimeColoredCurve::draw_colored_scatters(QCPPainter* painter, const QRectF& clip_rect)
{
    QCPAxis* keyAxis = mKeyAxis.data();
    QCPAxis* valueAxis = mValueAxis.data();

    applyScattersAntialiasingHint(painter);
    QCPScatterStyle style = mScatterStyle;
    const bool tint_brush = style.brush().style() != Qt::NoBrush;
    const int step = mScatterSkip + 1;

    int prev_bucket = -1;
    int index = 0;
    for (auto it = mDataContainer->constBegin(); it != mDataContainer->constEnd(); ++it, ++index)
    {
        if (index % step)
            continue;

        const QPointF pos(keyAxis->coordToPixel(it->key), valueAxis->coordToPixel(it->value));
        if (!clip_rect.contains(pos) || !qIsFinite(pos.x()) || !qIsFinite(pos.y()))
            continue;

        const int bucket = bucket_at(static_cast<int>(it->t));
        if (bucket != prev_bucket)
        {
            const QColor color = color_for_bucket(bucket);
            // Force the colour onto the style: applyTo() would otherwise keep an
            // explicitly-set marker pen and ignore the colour data entirely.
            QPen pen = style.isPenDefined() ? style.pen() : mPen;
            pen.setColor(color);
            style.setPen(pen);
            if (tint_brush)
                style.setBrush(color);
            style.applyTo(painter, pen);
            prev_bucket = bucket;
        }
        style.drawShape(painter, pos);
    }
}

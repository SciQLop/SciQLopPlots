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
#pragma once
#include <qcustomplot.h>
#include <QPointer>
#include <utility>
#include <array>
#include <optional>

/*!
 * \brief A QCPCurve whose segments and markers are tinted by a per-point scalar.
 *
 * The scalar is normalized over its own [min, max] and looked up in a
 * QCPColorGradient. Both entry points feed the same gradient: two-stop
 * (set_gradient_colors, used by the projection plots' time colouring) and full
 * QCPColorGradient (set_color_gradient, used by SciQLopCurve::set_color_data).
 */
class SciQLopTimeColoredCurve : public QCPCurve
{
    Q_OBJECT

    // Segments and markers are quantised to this many colours so the painter pen
    // is re-applied a handful of times per frame instead of once per point.
    static constexpr int color_buckets = 256;
    //! Bucket of a non-finite colour value: the segment or marker is not drawn.
    static constexpr int gap_bucket = -1;

    bool m_time_color_enabled = false;
    QCPColorGradient m_gradient;
    std::array<QRgb, color_buckets> m_lut {};
    //! Shared scale of the plot; read while an explicit scalar is set (see use_scale()).
    QPointer<QCPColorScale> m_scale;
    std::array<QRgb, color_buckets> m_scale_lut {};
    QVector<double> m_time_values;
    //! Explicit scalar. While empty the time values colour the curve instead.
    QVector<double> m_color_values;
    double m_c_min = 0.0;
    double m_c_max = 1.0;

public:
    explicit SciQLopTimeColoredCurve(QCPAxis* keyAxis, QCPAxis* valueAxis);

    void set_time_color_enabled(bool enabled) { m_time_color_enabled = enabled; }
    bool time_color_enabled() const { return m_time_color_enabled; }

    void set_time_values(const QVector<double>& times);
    void set_color_values(const QVector<double>& values);
    std::optional<QPointF> position_at_time(double t) const;

    /*! \brief Tint from \a start to \a end through a two-stop gradient. */
    void set_gradient_colors(const QColor& start, const QColor& end);
    void set_color_gradient(const QCPColorGradient& gradient);
    static QCPColorGradient two_stop_gradient(const QColor& start, const QColor& end);

    /*!
     * \brief set_color_scale Take range, log scale and gradient from \a scale
     *        instead of the curve's own, for as long as an explicit scalar is set.
     *        nullptr goes back to the curve's own.
     */
    void set_color_scale(QCPColorScale* scale);
    bool has_color_values() const { return !m_color_values.isEmpty(); }
    //! [min, max] of the finite explicit values; only positive ones when \a log.
    std::optional<std::pair<double, double>> color_range(bool log) const;

protected:
    void draw(QCPPainter* painter) override;

private:
    //! True when there is something to tint with — otherwise QCPCurve draws us.
    bool colouring_active() const noexcept
    {
        if (!m_time_color_enabled || active_values().isEmpty())
            return false;
        return use_scale() ? m_scale->dataRange().upper > m_scale->dataRange().lower
                           : m_c_max > m_c_min;
    }
    bool use_scale() const noexcept { return m_scale && !m_color_values.isEmpty(); }
    //! Position of \a value on the shared scale, NaN where it has no colour (log of <= 0).
    double scale_fraction(double value) const noexcept;
    void rebuild_scale_lut();
    void request_replot();
    const QVector<double>& active_values() const noexcept
    {
        return m_color_values.isEmpty() ? m_time_values : m_color_values;
    }
    //! Recomputes [m_c_min, m_c_max] over the finite active values.
    void update_range();
    //! Colour bucket of the data point at container index \a index.
    int bucket_at(int index) const noexcept;
    QColor color_for_bucket(int bucket) const;
    //! Bakes m_gradient into m_lut; call on every gradient change.
    void rebuild_lut();
    void draw_colored_line(QCPPainter* painter, const QRectF& clip_rect);
    void draw_colored_scatters(QCPPainter* painter, const QRectF& clip_rect);
};

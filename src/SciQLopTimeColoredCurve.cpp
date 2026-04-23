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

void SciQLopTimeColoredCurve::drawCurveLine(
    QCPPainter* painter, const QVector<QPointF>& lines) const
{
    if (!m_time_color_enabled || m_time_values.size() < 2 || lines.size() < 2)
    {
        QCPCurve::drawCurveLine(painter, lines);
        return;
    }

    const auto [t_min_it, t_max_it]
        = std::minmax_element(m_time_values.begin(), m_time_values.end());
    const double t_min = *t_min_it;
    const double t_max = *t_max_it;
    const double t_range = t_max - t_min;

    if (t_range <= 0.0)
    {
        QCPCurve::drawCurveLine(painter, lines);
        return;
    }

    QPen seg_pen = painter->pen();
    const auto n = std::min(lines.size(), static_cast<qsizetype>(m_time_values.size()));

    for (qsizetype i = 1; i < n; ++i)
    {
        const double f = (m_time_values[i] - t_min) / t_range;
        seg_pen.setColor(color_for_normalized(f));
        painter->setPen(seg_pen);
        painter->drawLine(lines[i - 1], lines[i]);
    }
}

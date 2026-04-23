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

class SciQLopTimeColoredCurve : public QCPCurve
{
    Q_OBJECT

    bool m_time_color_enabled = false;
    QColor m_gradient_start { 0, 0, 255 };
    QColor m_gradient_end { 255, 0, 0 };
    QVector<double> m_time_values;
    double m_t_min = 0.0;
    double m_t_max = 1.0;

public:
    using QCPCurve::QCPCurve;

    void set_time_color_enabled(bool enabled) { m_time_color_enabled = enabled; }
    bool time_color_enabled() const { return m_time_color_enabled; }

    void set_time_values(const QVector<double>& times);
    void set_gradient_colors(const QColor& start, const QColor& end)
    {
        m_gradient_start = start;
        m_gradient_end = end;
    }

protected:
    void draw(QCPPainter* painter) override;

private:
    QColor color_for_normalized(double f) const;
};

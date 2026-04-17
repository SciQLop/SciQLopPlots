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
#pragma once
#include <qcustomplot.h>
#include <QObject>
#include <limits>

class SciQLopCrosshair : public QObject
{
    Q_OBJECT

    QCPItemStraightLine* m_vline = nullptr;
    QCPItemStraightLine* m_hline = nullptr;
    QCPItemRichText* m_tooltip = nullptr;
    QCustomPlot* m_plot = nullptr;
    bool m_enabled = true;
    double m_current_key = std::numeric_limits<double>::quiet_NaN();
    double m_current_value = std::numeric_limits<double>::quiet_NaN();

public:
    explicit SciQLopCrosshair(QCustomPlot* plot);
    ~SciQLopCrosshair();

    void set_enabled(bool enabled);
    bool enabled() const { return m_enabled; }

    void update_at_key(double key);
    void update_at_pixel(const QPointF& pos);
    void hide();

    double current_key() const { return m_current_key; }
    void replot();

    void apply_theme(const QCPTheme* theme);

private:
    void update_vline_and_tooltip(double key, double value = std::numeric_limits<double>::quiet_NaN(),
                                  const QPointF& pixelPos = QPointF(std::numeric_limits<double>::quiet_NaN(),
                                                                    std::numeric_limits<double>::quiet_NaN()));
    QString build_tooltip_html(double key, const QPointF& pixelPos) const;
};

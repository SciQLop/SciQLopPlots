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

#include "SciQLopPlots/SciQLopPlotLegendInterface.hpp"

class QCPLegend;

class SciQLopPlotLegend : public SciQLopPlotLegendInterface
{
    Q_OBJECT

    QCPLegend* m_legend;

public:
    SciQLopPlotLegend(QCPLegend* legend, QObject* parent = nullptr);

    virtual ~SciQLopPlotLegend() = default;

    bool is_visible() const override;

    void set_visible(bool visible) override;

    //! The legend's top-left corner, in fractions (0 to 1) of the plot area.
    QPointF position() const override;
    //! Places the legend's top-left corner there, at its natural size.
    void set_position(const QPointF& position) override;

    void set_alignment(Qt::Alignment alignment) override;
    Qt::Alignment alignment() const override;
    void set_columns(int columns) override;
    int columns() const override;

    QFont font() const override;
    void set_font(const QFont& font) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    QStringList row_labels() const override;
    QRect row_rect(int row) const override;
};

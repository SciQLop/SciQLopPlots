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
#pragma once

#include "impl/SpanBase.hpp"
#include <QColor>
#include <QPointer>
#include <QString>
#include <qcustomplot.h>

class SciQLopSpanBase
{
    impl::SpanBase* _impl_base;
    QPointer<QCPAbstractSpanItem> _span;

public:
    explicit SciQLopSpanBase(impl::SpanBase* impl_base);
    ~SciQLopSpanBase();

    void set_visible(bool visible);
    [[nodiscard]] bool visible() const;

    void set_color(const QColor& color);
    [[nodiscard]] QColor color() const;

    void set_borders_color(const QColor& color);
    [[nodiscard]] QColor borders_color() const;

    void set_line_width(double width);
    [[nodiscard]] double line_width() const;

    void set_line_style(Qt::PenStyle style);
    [[nodiscard]] Qt::PenStyle line_style() const;

    void set_selected(bool selected);
    [[nodiscard]] bool selected() const;

    void set_read_only(bool read_only);
    [[nodiscard]] bool read_only() const;

    void set_tool_tip(const QString& tool_tip);
    [[nodiscard]] QString tool_tip() const;

    void replot();
    [[nodiscard]] QCustomPlot* parentPlot() const;
};

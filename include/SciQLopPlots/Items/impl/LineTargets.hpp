/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Items/SciQLopStraightLines.hpp"

#include "SciQLopPlots/Items/impl/PanelParts.hpp"

#include <QPointer>
#include <optional>

namespace impl
{
//! What a SciQLopStraightLine reads and writes: one plot's item, or a panel's parts.
class LineTarget
{
public:
    virtual ~LineTarget() = default;

    virtual double position() const = 0;
    //! Applies \a position, clamped; position_changed fires once, never for the current one.
    virtual void set_position(double position) = 0;
    virtual Coordinates coordinates() const = 0;
    //! Re-derives the position in \a coordinates, keeping the line in place. Silent.
    virtual void set_coordinates(Coordinates coordinates) = 0;
    virtual bool read_only() const = 0;
    virtual void set_read_only(bool read_only) = 0;
    virtual QString tool_tip() const = 0;
    virtual void set_tool_tip(const QString& tool_tip) = 0;
    virtual QColor color() const = 0;
    virtual void set_color(const QColor& color) = 0;
    virtual double line_width() const = 0;
    virtual void set_line_width(double width) = 0;
    virtual Qt::PenStyle line_style() const = 0;
    virtual void set_line_style(Qt::PenStyle style) = 0;
    virtual bool visible() const = 0;
    virtual void set_visible(bool visible) = 0;
    virtual bool selected() const = 0;
    virtual void set_selected(bool selected) = 0;
    virtual std::optional<double> min_value() const = 0;
    virtual void set_min_value(std::optional<double> min) = 0;
    virtual std::optional<double> max_value() const = 0;
    virtual void set_max_value(std::optional<double> max) = 0;
};

//! A line on one plot: one StraightLine item, which holds the state.
class PlotLineTarget final : public LineTarget
{
    QPointer<StraightLine> m_line;

public:
    PlotLineTarget(SciQLopStraightLine* line, SciQLopPlot* plot, double position, bool movable,
                   Coordinates coordinates, Qt::Orientation orientation);
    ~PlotLineTarget() override;

    double position() const override;
    void set_position(double position) override;
    Coordinates coordinates() const override;
    void set_coordinates(Coordinates coordinates) override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    double line_width() const override;
    void set_line_width(double width) override;
    Qt::PenStyle line_style() const override;
    void set_line_style(Qt::PenStyle style) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
    std::optional<double> min_value() const override;
    void set_min_value(std::optional<double> min) override;
    std::optional<double> max_value() const override;
    void set_max_value(std::optional<double> max) override;
};

//! Everything a panel line keeps and writes to its parts.
struct LineState
{
    double position = 0.;
    Qt::Orientation orientation = Qt::Vertical;
    Coordinates coordinates = Coordinates::Data;
    bool read_only = true;
    QString tool_tip;
    QColor color = QColor(0, 0, 0);
    double line_width = 1.0;
    Qt::PenStyle line_style = Qt::SolidLine;
    bool visible = true;
    bool selected = false;
    std::optional<double> min_value;
    std::optional<double> max_value;
};

//! A line on every SciQLopPlot of a panel: the state, and one plot-scope part per plot.
class PanelLineTarget final : public LineTarget
{
    SciQLopStraightLine* m_line;
    bool m_legacy_parts;
    LineState m_state;
    PanelParts<SciQLopStraightLine> m_parts; // last: destroyed first, while the state still exists

    SciQLopStraightLine* make_part(SciQLopPlot* plot);
    void follow(SciQLopStraightLine* part);
    template <typename T, typename Setter>
    void write(T& field, const T& value, Setter setter);

public:
    PanelLineTarget(SciQLopStraightLine* line, SciQLopMultiPlotPanel* panel, LineState state,
                    bool legacy_parts);

    double position() const override;
    void set_position(double position) override;
    Coordinates coordinates() const override;
    void set_coordinates(Coordinates coordinates) override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    double line_width() const override;
    void set_line_width(double width) override;
    Qt::PenStyle line_style() const override;
    void set_line_style(Qt::PenStyle style) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
    std::optional<double> min_value() const override;
    void set_min_value(std::optional<double> min) override;
    std::optional<double> max_value() const override;
    void set_max_value(std::optional<double> max) override;
};
} // namespace impl

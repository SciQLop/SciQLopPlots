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
#pragma once

#include "SciQLopPlots/Items/SciQLopSpan.hpp"
#include "SciQLopPlots/Items/impl/PanelParts.hpp"
#include "SciQLopPlots/Items/impl/SpanBase.hpp"

#include <QPointer>
#include <qcustomplot.h>
#include <vector>

namespace impl
{
//! The axes a span of \a kind bounds, key first.
inline std::vector<SpanAxis> bounded_axes(SpanKind kind)
{
    switch (kind)
    {
        case SpanKind::Vertical:
            return { SpanAxis::Key };
        case SpanKind::Horizontal:
            return { SpanAxis::Value };
        case SpanKind::Rectangular:
            return { SpanAxis::Key, SpanAxis::Value };
    }
    return {};
}

//! What a SciQLopSpan reads and writes: one plot's item, or a panel's parts.
class SpanTarget
{
public:
    virtual ~SpanTarget() = default;

    virtual SciQLopPlotRange range(SpanAxis axis) const = 0;
    //! Applies \a range, and the span notifies once; nothing for the current range.
    virtual void set_range(SpanAxis axis, const SciQLopPlotRange& range) = 0;
    virtual SciQLopPlotRange pixel_range(SpanAxis axis) const = 0;
    //! \a pixels (from the plot area) in the span's coordinates.
    virtual SciQLopPlotRange to_range(SpanAxis axis, const SciQLopPlotRange& pixels) const = 0;
    virtual Coordinates coordinates() const = 0;
    //! Re-expresses every bounded axis in \a coordinates, keeping the span in place. Silent: the
    //! span reports what changed.
    virtual void set_coordinates(Coordinates coordinates) = 0;
    virtual bool read_only() const = 0;
    virtual void set_read_only(bool read_only) = 0;
    virtual QString tool_tip() const = 0;
    virtual void set_tool_tip(const QString& tool_tip) = 0;
    virtual QColor borders_color() const = 0;
    virtual void set_borders_color(const QColor& color) = 0;
    virtual double line_width() const = 0;
    virtual void set_line_width(double width) = 0;
    virtual Qt::PenStyle line_style() const = 0;
    virtual void set_line_style(Qt::PenStyle style) = 0;
    virtual QColor color() const = 0;
    virtual void set_color(const QColor& color) = 0;
    virtual bool visible() const = 0;
    virtual void set_visible(bool visible) = 0;
    virtual bool selected() const = 0;
    virtual void set_selected(bool selected) = 0;
};

//! A span on one plot: one NeoQCP span item, which holds the state.
class PlotSpanTarget final : public SpanTarget
{
    SpanKind m_kind;
    QPointer<QCPAbstractSpanItem> m_item;
    SpanBase* m_base; // m_item seen as a SpanBase: use it only while m_item is alive
    Coordinates m_coordinates;

public:
    PlotSpanTarget(SciQLopSpan* span, SciQLopPlot* plot, SpanKind kind,
                   const SciQLopPlotRange& key_range, const SciQLopPlotRange& value_range,
                   Coordinates coordinates);
    ~PlotSpanTarget() override;

    SciQLopPlotRange range(SpanAxis axis) const override;
    void set_range(SpanAxis axis, const SciQLopPlotRange& range) override;
    SciQLopPlotRange pixel_range(SpanAxis axis) const override;
    SciQLopPlotRange to_range(SpanAxis axis, const SciQLopPlotRange& pixels) const override;
    Coordinates coordinates() const override;
    void set_coordinates(Coordinates coordinates) override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor borders_color() const override;
    void set_borders_color(const QColor& color) override;
    double line_width() const override;
    void set_line_width(double width) override;
    Qt::PenStyle line_style() const override;
    void set_line_style(Qt::PenStyle style) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
};

//! Everything a panel span keeps and writes to its parts.
struct SpanState
{
    SciQLopPlotRange key_range;
    SciQLopPlotRange value_range;
    Coordinates coordinates = Coordinates::Data;
    bool read_only = false;
    QString tool_tip;
    QColor color;
    QColor borders_color;
    double line_width = 3.0;
    Qt::PenStyle line_style = Qt::SolidLine;
    bool visible = true;
    bool selected = false;
};

//! A span on every SciQLopPlot of a panel: the state, and one plot-scope part per plot.
class PanelSpanTarget final : public SpanTarget
{
    SciQLopSpan* m_span;
    SpanKind m_kind;
    bool m_legacy_parts;
    SpanState m_state;
    PanelParts<SciQLopSpan> m_parts; // last: destroyed first, while the state still exists

    SciQLopSpan* make_part(SciQLopPlot* plot);
    void follow(SciQLopSpan* part);
    //! Stores \a range and writes it to every part; false when a slot deleted the span meanwhile.
    bool store_range(SpanAxis axis, const SciQLopPlotRange& range);
    template <typename T, typename Setter>
    void write(T& field, const T& value, Setter setter);

public:
    PanelSpanTarget(SciQLopSpan* span, SciQLopMultiPlotPanel* panel, SpanKind kind,
                    SpanState state, bool legacy_parts);

    SciQLopPlotRange range(SpanAxis axis) const override;
    void set_range(SpanAxis axis, const SciQLopPlotRange& range) override;
    SciQLopPlotRange pixel_range(SpanAxis axis) const override;
    SciQLopPlotRange to_range(SpanAxis axis, const SciQLopPlotRange& pixels) const override;
    Coordinates coordinates() const override;
    void set_coordinates(Coordinates coordinates) override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    QColor borders_color() const override;
    void set_borders_color(const QColor& color) override;
    double line_width() const override;
    void set_line_width(double width) override;
    Qt::PenStyle line_style() const override;
    void set_line_style(Qt::PenStyle style) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
};
} // namespace impl

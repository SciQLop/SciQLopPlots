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

#include "SciQLopPlots/SciQLopDrawnObject.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"

#include <QColor>
#include <QString>
#include <memory>
#include <optional>

class SciQLopPlot;
class SciQLopMultiPlotPanel;

namespace impl
{
//! The axis a span range lies on: the key axis (x) or the value axis (y).
enum class SpanAxis
{
    Key,
    Value
};
class SpanTarget;
class PlotSpanTarget;
class PanelSpanTarget;
}

namespace SciQLopPlots
{
//! The kind of a span bounding the key axis, the value axis or both; std::invalid_argument for
//! neither.
SpanKind span_kind(bool bounds_key, bool bounds_value);
}

/*!
 * \brief A band over the key axis (Vertical), the value axis (Horizontal) or both (Rectangular),
 * on one plot or on every plot of a panel.
 *
 * Kind and scope are fixed at creation; plot.span(...), panel.span(...) and their shorthands make
 * them (docs/plans/2026-10-25-span-line-unification-design.md). Inside, a target holds the state:
 * one NeoQCP item on a plot, or one plot-scope part per plot on a panel.
 */
class SciQLopSpan : public SciQLopDrawnObject
{
    Q_OBJECT
    friend class impl::PlotSpanTarget;
    friend class impl::PanelSpanTarget;

    SpanKind m_kind;
    QString m_id;
    std::unique_ptr<impl::SpanTarget> m_target;

    [[nodiscard]] bool bounds(impl::SpanAxis axis) const noexcept;
    void require_bounded(impl::SpanAxis axis, const char* what) const;
    [[nodiscard]] impl::SpanAxis one_axis(const char* what) const;
    //! The axis's signal, then range_changed on a one-axis span: one change, one of each.
    void notify_range(impl::SpanAxis axis, const SciQLopPlotRange& range);

protected:
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override;
    void _set_selected(bool selected) override;
    QColor _color() const override;
    void _set_color(const QColor& color) override;

    //! The old SciQLopVerticalSpan's set_pixel_range: \a pixels from the plot area (Plan B removes it).
    void _set_pixel_range(impl::SpanAxis axis, const SciQLopPlotRange& pixels);

public:
    SciQLopSpan(SciQLopPlot* plot, SpanKind kind, const SciQLopPlotRange& key_range,
                const SciQLopPlotRange& value_range, const QColor& color, bool read_only,
                bool visible, const QString& tool_tip, Coordinates coordinates,
                const QString& id = "");
    //! \a legacy_parts: parts are SciQLopVerticalSpans, for the old MultiPlotsVerticalSpan (Plan A).
    SciQLopSpan(SciQLopMultiPlotPanel* panel, SpanKind kind, const SciQLopPlotRange& key_range,
                const SciQLopPlotRange& value_range, const QColor& color, bool read_only,
                bool visible, const QString& tool_tip, Coordinates coordinates,
                const QString& id = "", bool legacy_parts = false);
    ~SciQLopSpan() override;

    [[nodiscard]] SpanKind kind() const noexcept;
    //! The plot or the panel the span belongs to.
    [[nodiscard]] QObject* scope() const;
    //! True for the per-plot parts of a panel-scope span.
    [[nodiscard]] bool is_panel_part() const;
    [[nodiscard]] QString id() const;

    //! The range of the key (x) or value (y) axis; std::nullopt (None) on an axis the span does
    //! not bound.
    [[nodiscard]] std::optional<SciQLopPlotRange> key_range() const;
    [[nodiscard]] std::optional<SciQLopPlotRange> value_range() const;

    void set_key_range(const SciQLopPlotRange& range);
    void set_value_range(const SciQLopPlotRange& range);
    //! The bounded axis of a vertical or horizontal span; std::invalid_argument on a rectangular one.
    [[nodiscard]] SciQLopPlotRange range() const;
    void set_range(const SciQLopPlotRange& range);

    [[nodiscard]] Coordinates coordinates() const;
    //! Data, or Pixels from the plot area's top-left, for every bounded axis; the span stays put.
    void set_coordinates(Coordinates coordinates);
    [[nodiscard]] SciQLopPlotRange pixel_range() const;
    [[nodiscard]] SciQLopPlotRange pixel_key_range() const;
    [[nodiscard]] SciQLopPlotRange pixel_value_range() const;

    [[nodiscard]] bool read_only() const;
    void set_read_only(bool read_only);
    [[nodiscard]] QString tool_tip() const;
    void set_tool_tip(const QString& tool_tip);

    //! The border pen: colour, width and style.
    [[nodiscard]] QColor borders_color() const;
    void set_borders_color(const QColor& color);
    [[nodiscard]] double line_width() const;
    void set_line_width(double width);
    [[nodiscard]] Qt::PenStyle line_style() const;
    void set_line_style(Qt::PenStyle style);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange range);
    Q_SIGNAL void key_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void value_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void coordinates_changed(Coordinates coordinates);
    Q_SIGNAL void read_only_changed(bool read_only);
    Q_SIGNAL void tool_tip_changed(const QString& tool_tip);
    Q_SIGNAL void delete_requested();
};

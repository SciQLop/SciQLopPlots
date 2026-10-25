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
    virtual Coordinates coordinates() const = 0;
    virtual bool read_only() const = 0;
    virtual void set_read_only(bool read_only) = 0;
    virtual QString tool_tip() const = 0;
    virtual void set_tool_tip(const QString& tool_tip) = 0;
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
    Coordinates coordinates() const override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
};
} // namespace impl

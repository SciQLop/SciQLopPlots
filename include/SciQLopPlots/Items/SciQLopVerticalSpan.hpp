/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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

#include "../MultiPlots/SciQLopMultiPlotObject.hpp"
#include "SciQLopSpanBase.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/helpers.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>

namespace impl
{

class VerticalSpan : public QCPItemVSpan, public impl::SpanBase
{
    Q_OBJECT

public:
    VerticalSpan(QCustomPlot* plot, SciQLopPlotRange horizontal_range,
                 bool do_not_replot = false, bool immediate_replot = false);

    using SpanBase::set_visible;
    using SpanBase::set_color;
    using SpanBase::color;
    using SpanBase::set_borders_color;
    using SpanBase::borders_color;
    using SpanBase::set_borders_tool_tip;
    using SpanBase::replot;

    void set_range(const SciQLopPlotRange horizontal_range);
    [[nodiscard]] SciQLopPlotRange range() const noexcept;

    void select_lower_border(bool selected);
    void select_upper_border(bool selected);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);
    Q_SIGNAL void delete_requested();
};

} // namespace impl

/*! \brief Vertical span that can be added to a plot
 *
 */
class SciQLopVerticalSpan : public SciQLopRangeItemInterface
{
    Q_OBJECT
    QPointer<impl::VerticalSpan> _impl;
    SciQLopSpanBase _base;

    friend class MultiPlotsVerticalSpan;

protected:
    inline void select_lower_border(bool selected)
    {
        qptr_apply(_impl, [selected](auto& item) { item->select_lower_border(selected); });
    }

    inline void select_upper_border(bool selected)
    {
        qptr_apply(_impl, [selected](auto& item) { item->select_upper_border(selected); });
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);

public:
    SciQLopVerticalSpan(SciQLopPlot* plot, SciQLopPlotRange horizontal_range,
                        QColor color = QColor(100, 100, 100, 100), bool read_only = false,
                        bool visible = true, const QString& tool_tip = "");

    virtual ~SciQLopVerticalSpan() override = default;

    inline QCustomPlot* parentPlot() const noexcept { return _base.parentPlot(); }

    inline void set_visible(bool visible) noexcept override { _base.set_visible(visible); }
    inline bool visible() const noexcept override { return _base.visible(); }

    inline void set_range(const SciQLopPlotRange& horizontal_range) noexcept override
    {
        qptr_apply(_impl, [horizontal_range](auto& item) { item->set_range(horizontal_range); });
    }

    [[nodiscard]] inline SciQLopPlotRange range() const noexcept override
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->range(); });
    }

    inline void set_color(const QColor& color) { _base.set_color(color); }
    [[nodiscard]] inline QColor color() const { return _base.color(); }

    inline void set_borders_color(const QColor& color) { _base.set_borders_color(color); }
    [[nodiscard]] inline QColor borders_color() const noexcept { return _base.borders_color(); }

    inline void set_selected(bool selected) { _base.set_selected(selected); }
    [[nodiscard]] inline bool selected() const noexcept { return _base.selected(); }

    inline void set_read_only(bool read_only) { _base.set_read_only(read_only); }
    [[nodiscard]] inline bool read_only() const noexcept { return _base.read_only(); }

    inline void set_tool_tip(const QString& tool_tip) noexcept override { _base.set_tool_tip(tool_tip); }
    [[nodiscard]] inline QString tool_tip() const noexcept override { return _base.tool_tip(); }

    inline void replot() override { _base.replot(); }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void selectionChanged(bool);
    Q_SIGNAL void delete_requested();
};

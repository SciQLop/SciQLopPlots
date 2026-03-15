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
#include "SciQLopPlotItem.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/helpers.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>

namespace impl
{

class VerticalSpan : public QCPItemVSpan, public impl::SciQlopItemWithToolTip
{
    Q_OBJECT

public:
    VerticalSpan(QCustomPlot* plot, SciQLopPlotRange horizontal_range,
                 bool do_not_replot = false, bool immediate_replot = false);

    void set_visible(bool visible);
    void set_range(const SciQLopPlotRange horizontal_range);
    [[nodiscard]] SciQLopPlotRange range() const noexcept;

    void set_color(const QColor& color);
    [[nodiscard]] QColor color() const noexcept;

    void set_borders_color(const QColor& color);
    [[nodiscard]] QColor borders_color() const noexcept;

    void set_borders_tool_tip(const QString& tool_tip);

    void select_lower_border(bool selected);
    void select_upper_border(bool selected);

    inline void replot(bool immediate = false)
    {
        if (immediate)
        {
            if (auto* l = this->layer())
                l->replot();
        }
        else
            parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }

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

    friend class MultiPlotsVerticalSpan;

protected:
    inline void select_lower_border(bool selected)
    {
        qptr_apply(_impl, [&selected](auto& item) { item->select_lower_border(selected); });
    }

    inline void select_upper_border(bool selected)
    {
        qptr_apply(_impl, [&selected](auto& item) { item->select_upper_border(selected); });
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);

public:
    /*! \brief SciQLopVerticalSpan
     *
     * \param plot The plot where the vertical span will be added
     * \param horizontal_range The range of the span
     * \param color The color of the span
     * \param read_only If the span is read only
     * \param visible If the span is visible
     * \param tool_tip The tool tip of the span
     */
    SciQLopVerticalSpan(SciQLopPlot* plot, SciQLopPlotRange horizontal_range,
                        QColor color = QColor(100, 100, 100, 100), bool read_only = false,
                        bool visible = true, const QString& tool_tip = "");

    virtual ~SciQLopVerticalSpan() override
    {
        qptr_apply(_impl,
                   [this](auto& item)
                   {
                       auto plot = this->parentPlot();
                       if (plot->hasItem(this->_impl.data()))
                       {
                           plot->removeItem(this->_impl.data());
                           plot->replot(QCustomPlot::rpQueuedReplot);
                       }
                   });
    }

    inline QCustomPlot* parentPlot() const noexcept
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->parentPlot(); }, nullptr);
    }

    /*! \brief Set the span as movable
     *
     * \param movable
     */
    inline void set_visible(bool visible) noexcept override
    {
        qptr_apply(_impl, [&visible](auto& item) { item->set_visible(visible); });
    }

    /*! \brief Set the span as movable
     *
     * \param movable
     */
    inline bool visible() const noexcept override
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->visible(); });
    }

    inline void set_range(const SciQLopPlotRange& horizontal_range) noexcept override
    {
        qptr_apply(_impl, [&horizontal_range](auto& item) { item->set_range(horizontal_range); });
    }

    [[nodiscard]] inline SciQLopPlotRange range() const noexcept override
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->range(); });
    }

    inline void set_color(const QColor& color)
    {
        return qptr_apply(_impl, [&color](auto& item) { item->set_color(color); });
    }

    [[nodiscard]] inline QColor color() const
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->color(); });
    }

    inline void set_borders_color(const QColor& color)
    {
        qptr_apply(_impl, [&color](auto& item) { item->set_borders_color(color); });
    }

    [[nodiscard]] inline QColor borders_color() const noexcept
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->borders_color(); });
    }

    inline void set_selected(bool selected)
    {
        qptr_apply(_impl, [&selected](auto& item) { item->setSelected(selected); });
    }

    [[nodiscard]] inline bool selected() const noexcept
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->selected(); });
    }

    inline void set_read_only(bool read_only)
    {
        qptr_apply(_impl, [&read_only](auto& item) { item->setMovable(!read_only); });
    }

    [[nodiscard]] inline bool read_only() const noexcept
    {
        return qptr_apply_or(_impl, [](auto& item) { return !item->movable(); });
    }

    inline void set_tool_tip(const QString& tool_tip) noexcept override
    {
        qptr_apply(_impl,
                   [&tool_tip](auto& item)
                   {
                       item->setToolTip(tool_tip);
                       item->set_borders_tool_tip(tool_tip);
                   });
    }

    [[nodiscard]] inline QString tool_tip() const noexcept override
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->tooltip(); });
    }

    inline void replot() override
    {
        qptr_apply(_impl, [](auto& item) { item->replot(); });
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void selectionChanged(bool);
    Q_SIGNAL void delete_requested();
};

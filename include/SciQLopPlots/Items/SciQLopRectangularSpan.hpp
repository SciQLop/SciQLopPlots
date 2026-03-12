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

#include "SciQLopPlotItem.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/helpers.hpp"
#include <QBrush>
#include <QColor>
#include <qcustomplot.h>

class SciQLopPlot;

namespace impl
{

class RectangularSpan : public QCPItemRSpan, public impl::SciQlopItemWithToolTip
{
    Q_OBJECT

public:
    RectangularSpan(QCustomPlot* plot, SciQLopPlotRange key_range, SciQLopPlotRange value_range,
                    bool do_not_replot = false, bool immediate_replot = false);

    void set_visible(bool visible);

    void set_key_range(const SciQLopPlotRange range);
    [[nodiscard]] SciQLopPlotRange key_range() const noexcept;

    void set_value_range(const SciQLopPlotRange range);
    [[nodiscard]] SciQLopPlotRange value_range() const noexcept;

    void set_color(const QColor& color);
    [[nodiscard]] QColor color() const noexcept;

    void set_borders_color(const QColor& color);
    [[nodiscard]] QColor borders_color() const noexcept;

    void set_borders_tool_tip(const QString& tool_tip);

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
    Q_SIGNAL void key_range_changed(SciQLopPlotRange new_range);
    Q_SIGNAL void value_range_changed(SciQLopPlotRange new_range);
    Q_SIGNAL void delete_requested();
};

} // namespace impl

class SciQLopRectangularSpan : public SciQLopMovableItemInterface
{
    Q_OBJECT
    QPointer<impl::RectangularSpan> _impl;

public:
    SciQLopRectangularSpan(SciQLopPlot* plot, SciQLopPlotRange key_range,
                           SciQLopPlotRange value_range,
                           QColor color = QColor(100, 100, 100, 100), bool read_only = false,
                           bool visible = true, const QString& tool_tip = "");

    virtual ~SciQLopRectangularSpan() override
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

    inline void set_visible(bool visible) noexcept override
    {
        qptr_apply(_impl, [&visible](auto& item) { item->set_visible(visible); });
    }

    inline bool visible() const noexcept override
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->visible(); });
    }

    inline void set_key_range(const SciQLopPlotRange& range) noexcept
    {
        qptr_apply(_impl, [&range](auto& item) { item->set_key_range(range); });
    }

    [[nodiscard]] inline SciQLopPlotRange key_range() const noexcept
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->key_range(); });
    }

    inline void set_value_range(const SciQLopPlotRange& range) noexcept
    {
        qptr_apply(_impl, [&range](auto& item) { item->set_value_range(range); });
    }

    [[nodiscard]] inline SciQLopPlotRange value_range() const noexcept
    {
        return qptr_apply_or(_impl, [](auto& item) { return item->value_range(); });
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
    Q_SIGNAL void key_range_changed(SciQLopPlotRange new_range);
    Q_SIGNAL void value_range_changed(SciQLopPlotRange new_range);
    Q_SIGNAL void selectionChanged(bool);
    Q_SIGNAL void delete_requested();
};

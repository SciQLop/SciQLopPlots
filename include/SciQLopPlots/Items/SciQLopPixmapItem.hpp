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

#include "SciQLopPlots/ThreadGuard.hpp"

#include "SciQLopPlotItem.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/enums.hpp"
#include "SciQLopPlots/helpers.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>

namespace impl
{
class PixmapItem : public impl::SciQLopPlotItem<QCPItemPixmap>, public impl::SciQlopItemWithToolTip
{
    Q_OBJECT

public:

    inline PixmapItem(QCustomPlot* plot, const QPixmap& pixmap, const QRectF& rect,
                      bool movable = false, Coordinates coordinates = Coordinates::Pixels)
            : impl::SciQLopPlotItem<QCPItemPixmap> { plot }
    {
        this->setPixmap(pixmap);
        // setType before setCoords: setType retains the pixel position computed
        // under the current type, so coords set first (default plot-coords) then
        // switched would be converted off-screen. Matches EllipseItem/TextItem
        // ordering.
        if (coordinates == Coordinates::Data)
        {
            this->topLeft->setType(QCPItemPosition::ptPlotCoords);
            this->bottomRight->setType(QCPItemPosition::ptPlotCoords);
        }
        else
        {
            // Pixels are measured from the plot area, not the widget: ptAbsolute's
            // origin sits inside the axis margins, where clipToAxisRect() hides it.
            this->topLeft->setType(QCPItemPosition::ptAxisRectAbsolute);
            this->bottomRight->setType(QCPItemPosition::ptAxisRectAbsolute);
        }
        this->topLeft->setCoords(rect.topLeft());
        this->bottomRight->setCoords(rect.bottomRight());
        this->setMovable(movable);
    }

    virtual ~PixmapItem() { }

    virtual void move(double dx, double dy) override;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void moved(double new_x, double new_y);
};
} // namespace impl

/*!
 * \brief The SciQLopPixmapItem class
 */
class SciQLopPixmapItem : public SciQLopBoundingRectItemInterface
{
    Q_OBJECT

    QPointer<impl::PixmapItem> m_item;

public:
    /*!
     * \brief SciQLopPixmapItem
     * \param plot The plot to which the item will be added
     * \param pixmap The pixmap to display
     * \param rect The rectangle in which to display the pixmap
     * \param movable If the pixmap can be moved by the user
     * \param coordinates The coordinates system in which the rectangle is defined (Pixels or Data)
     */
    SciQLopPixmapItem(SciQLopPlot* plot, const QPixmap& pixmap, const QRectF& rect,
                      bool movable = false, Coordinates coordinates = Coordinates::Pixels)
            : SciQLopBoundingRectItemInterface(plot)
    {
        SciQLopPlots::require_owner_thread(plot, "SciQLopPixmapItem");
        if (plot == nullptr)
            throw std::invalid_argument("plot cannot be nullptr");
        else
            m_item = new impl::PixmapItem(plot->qcp_plot(), pixmap, rect, movable, coordinates);
        _follow_selection(m_item);
    }

    virtual ~SciQLopPixmapItem() { qcp_item_remove(m_item); }

    //! The top-left corner, in the item's coordinates.
    [[nodiscard]] inline QPointF position() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->topLeft->coords(); });
    }

    //! Moves the pixmap, keeping its size.
    inline void set_position(const QPointF& pos) noexcept override
    {
        SCIQLOP_ON_OWNER_THREAD(set_position(pos));
        qptr_apply(m_item,
                   [pos](auto&& item)
                   {
                       const auto shift = pos - item->topLeft->coords();
                       item->topLeft->setCoords(pos);
                       item->bottomRight->setCoords(item->bottomRight->coords() + shift);
                       item->replot();
                   });
    }

    [[nodiscard]] inline QRectF bounding_rectangle() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item)
                             { return QRectF(item->topLeft->coords(), item->bottomRight->coords()); });
    }

    inline void set_movable(bool movable) noexcept override
    {
        SCIQLOP_ON_OWNER_THREAD(set_movable(movable));
        qptr_apply(m_item, [movable](auto&& item) { item->setMovable(movable); });
    }

    [[nodiscard]] inline bool movable() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->movable(); }, false);
    }

    [[nodiscard]] inline QString tool_tip() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->tooltip(); });
    }

    inline void set_tool_tip(const QString& tool_tip) noexcept override
    {
        SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
        qptr_apply(m_item, [tool_tip](auto&& item) { item->setToolTip(tool_tip); });
    }

protected:
    void _set_visible(bool visible) override
    {
        qptr_apply(m_item, [visible](auto&& item) { item->setVisible(visible); item->replot(); });
    }

    bool _visible() const override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->visible(); }, false);
    }

    void _set_selected(bool selected) override
    {
        qptr_apply(m_item, [selected](auto&& item) { item->setSelected(selected); item->replot(); });
    }

    bool _selected() const override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->selected(); }, false);
    }
};

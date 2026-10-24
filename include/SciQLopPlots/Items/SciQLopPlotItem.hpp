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
#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/SciQLopDrawnObject.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include "SciQLopPlots/enums.hpp"
#include "SciQLopPlots/helpers.hpp"
#include <QPointer>
#include <QTimer>
#include <qcustomplot.h>
#include <stdexcept>
#include <string>

namespace SciQLopPlots
{
//! The plot an item is built on, for the item's first base-class initializer: the item's
//! QObject parent, checked before anything is built on it.
template <typename Plot>
Plot* item_plot(Plot* plot, const char* what)
{
    if (plot == nullptr)
        throw std::invalid_argument(std::string(what) + ": plot cannot be None");
    return on_owner_thread(plot, what);
}
}

namespace impl
{
class SciQLopPlotItemBase
{
public:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) = 0;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) = 0;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) = 0;
    virtual QCursor cursor(QMouseEvent* event) const noexcept = 0;
    virtual void replot(bool immediate = false) = 0;
};

template <typename QCPAbstractItem_T>
class SciQLopPlotItem : public QCPAbstractItem_T, public SciQLopPlotItemBase
{
protected:
    bool _movable = false;
    bool _deletable = false;
    bool _queued_replot = false;
    QPointF _last_position;

public:
    SciQLopPlotItem(QCustomPlot* plot) : QCPAbstractItem_T { plot } { }

    virtual ~SciQLopPlotItem() { }

    inline bool movable() const noexcept { return this->_movable; }

    inline virtual void setMovable(bool movable) noexcept { this->_movable = movable; }

    virtual void move(double dx, double dy) = 0;

    inline void replot(bool immediate = false) override
    {
        if (immediate)
        {
            if (auto* l = this->layer())
                l->replot();
        }
        else
        {
            if (not _queued_replot)
            {
                _queued_replot = true;
                QTimer::singleShot(0, this,
                                   [this]()
                                   {
                                       if (auto* l = this->layer())
                                           l->replot();
                                       _queued_replot = false;
                                   });
            }
        }
    }

    inline void mousePressEvent(QMouseEvent* event, const QVariant& details) override
    {
        this->_last_position = event->pos();
        event->accept();
    }

    inline void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override
    {
        const QPointer<QCPAbstractItem_T> self(this);
        if (_movable and event->buttons() == Qt::LeftButton)
        {
            move(event->position().x() - this->_last_position.x(),
                 event->position().y() - this->_last_position.y());
        }
        // move() may emit: a slot may have deleted this item.
        if (!self)
            return;
        this->_last_position = event->position();
        event->accept();
    }

    inline void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override { }

    virtual QCursor cursor(QMouseEvent* event) const noexcept override { return Qt::ArrowCursor; }
};

class SciQlopItemWithToolTip
{
    QString _tooltip;

public:
    inline SciQlopItemWithToolTip() = default;

    inline SciQlopItemWithToolTip(const QString& tooltip) : _tooltip { tooltip } { }

    inline QString tooltip() const noexcept { return _tooltip; }

    inline void setToolTip(const QString& tooltip) noexcept { _tooltip = tooltip; }
};

} // namespace impl

class SciQLopItemInterface : public SciQLopDrawnObject
{
    Q_OBJECT

    QPointer<QCPAbstractItem> m_qcp_item;

    static void _replot(QCPAbstractItem* item)
    {
        if (auto* own = dynamic_cast<impl::SciQLopPlotItemBase*>(item))
            own->replot();
    }

public:
    SciQLopItemInterface(QObject* parent = nullptr) : SciQLopDrawnObject { parent } { }

    inline virtual ~SciQLopItemInterface() = default;

    inline virtual Coordinates coordinates() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return Coordinates::Data;
    }

    inline virtual void set_coordinates(Coordinates coordinates) { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual QString tool_tip() const noexcept { return ""; }
    inline virtual void set_tool_tip(const QString& toolTip) noexcept { WARN_ABSTRACT_METHOD; }

protected:
    inline virtual void replot() { WARN_ABSTRACT_METHOD; }

    //! NeoQCP selects an item on click: follow it, so selection_changed fires then too.
    //! The item is also the one the visible and selected hooks below act on.
    void _follow_selection(QCPAbstractItem* item)
    {
        m_qcp_item = item;
        if (item)
            connect(item, &QCPAbstractItem::selectionChanged, this,
                    &SciQLopItemInterface::_notify_selected, Qt::UniqueConnection);
    }

    void _set_visible(bool visible) override
    {
        qptr_apply(m_qcp_item, [visible](auto&& item) { item->setVisible(visible); _replot(item); });
    }

    bool _visible() const override
    {
        return qptr_apply_or(m_qcp_item, [](auto&& item) { return item->visible(); }, false);
    }

    void _set_selected(bool selected) override
    {
        qptr_apply(m_qcp_item, [selected](auto&& item) { item->setSelected(selected); _replot(item); });
    }

    bool _selected() const override
    {
        return qptr_apply_or(m_qcp_item, [](auto&& item) { return item->selected(); }, false);
    }

    inline virtual impl::SciQLopPlotItemBase* impl()
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }
};

class SciQLopMovableItemInterface : public SciQLopItemInterface
{
    Q_OBJECT

public:
    SciQLopMovableItemInterface(QObject* parent = nullptr) : SciQLopItemInterface { parent } { }

    inline virtual ~SciQLopMovableItemInterface() = default;

    inline virtual void move(double dx, double dy) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_movable(bool movable) noexcept { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual bool movable() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    [[nodiscard]] inline virtual QPointF position() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual void set_position(const QPointF& pos) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_position(double x, double y) noexcept
    {
        set_position(QPointF { x, y });
    }
};

class SciQLopLineItemInterface : public SciQLopItemInterface
{
    Q_OBJECT

public:
    SciQLopLineItemInterface(QObject* parent = nullptr) : SciQLopItemInterface { parent } { }

    inline virtual ~SciQLopLineItemInterface() = default;

    inline virtual void set_line_width(double width) { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual double line_width() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }
};

class SciQLopBoundingRectItemInterface : public SciQLopMovableItemInterface
{
    Q_OBJECT

public:
    SciQLopBoundingRectItemInterface(QObject* parent = nullptr)
            : SciQLopMovableItemInterface { parent }
    {
    }

    inline virtual ~SciQLopBoundingRectItemInterface() = default;

    inline virtual QRectF bounding_rectangle() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }
};

class SciQLopPolygonItemInterface : public SciQLopMovableItemInterface
{
    Q_OBJECT

public:
    SciQLopPolygonItemInterface(QObject* parent = nullptr) : SciQLopMovableItemInterface { parent }
    {
    }

    inline virtual ~SciQLopPolygonItemInterface() = default;

    inline virtual void set_brush(const QBrush& brush) noexcept { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual QBrush brush() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual void set_fill_color(const QColor& color) noexcept
    {
        QBrush brush = this->brush();
        brush.setColor(color);
        this->set_brush(brush);
    }

    [[nodiscard]] inline virtual QColor fill_color() const noexcept { return brush().color(); }

    inline virtual void set_pen(const QPen& pen) noexcept { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual QPen pen() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual QRectF bounding_rectangle() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }
};

class SciQLopRangeItemInterface : public SciQLopMovableItemInterface
{
    Q_OBJECT

public:

    SciQLopRangeItemInterface(QObject* parent = nullptr) : SciQLopMovableItemInterface { parent } { }

    inline virtual ~SciQLopRangeItemInterface() = default;

    inline virtual void set_range(const SciQLopPlotRange& range) noexcept { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual SciQLopPlotRange range() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
};

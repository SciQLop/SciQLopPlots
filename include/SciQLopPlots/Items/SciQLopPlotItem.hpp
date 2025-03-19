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
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"
#include <QTimer>
#include <qcustomplot.h>

namespace impl
{
class SciQLopPlotItemBase
{
public:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) = 0;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) = 0;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) = 0;
    virtual QCursor cursor(QMouseEvent* event) const noexcept = 0;
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

    inline virtual void replot(bool immediate = false)
    {
        if (immediate)
            this->layer()->replot();
        else
        {
            if (not _queued_replot)
            {
                _queued_replot = true;
                QTimer::singleShot(0,
                                   [this]()
                                   {
                                       this->layer()->replot();
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
        if (_movable and event->buttons() == Qt::LeftButton)
        {
            move(event->position().x() - this->_last_position.x(),
                 event->position().y() - this->_last_position.y());
        }
        this->_last_position = event->position();
        event->accept();
    }

    inline void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override { }

    virtual QCursor cursor(QMouseEvent* event) const noexcept override { return Qt::ArrowCursor; }
};

class SciQLopItemWithKeyInteraction
{
public:
    virtual void keyPressEvent(QKeyEvent* event) = 0;
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

class SciQLopItemInterface : public QObject
{
    Q_OBJECT

public:
    SciQLopItemInterface(QObject* parent = nullptr) : QObject { parent } { }

    inline virtual ~SciQLopItemInterface() = default;

    inline virtual void set_visible(bool visible) { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual bool visible() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

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

    inline virtual void set_color(const QColor& color) noexcept { WARN_ABSTRACT_METHOD; }

    [[nodiscard]] inline virtual QColor color() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

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

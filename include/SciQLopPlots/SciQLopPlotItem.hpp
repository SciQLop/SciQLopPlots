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
#include <QTimer>
#include <qcustomplot.h>

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

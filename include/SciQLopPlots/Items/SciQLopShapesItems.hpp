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

#include "SciQLopPlotItem.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/enums.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>

class EllipseItem : public SciQLopPlotItem<QCPItemEllipse>, public SciQlopItemWithToolTip
{
    Q_OBJECT

public:
#ifndef BINDINGS_H
    Q_SIGNAL void moved(double new_x, double new_y);
#endif // !BINDINGS_H

    inline EllipseItem(QCustomPlot* plot, const QRectF& boundingRectangle, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
            : SciQLopPlotItem<QCPItemEllipse> { plot }
    {
        this->setMovable(movable);
        if (coordinates == Coordinates::Data)
        {
            this->topLeft->setType(QCPItemPosition::ptPlotCoords);
            this->bottomRight->setType(QCPItemPosition::ptPlotCoords);
        }
        else
        {
            this->topLeft->setType(QCPItemPosition::ptAbsolute);
            this->bottomRight->setType(QCPItemPosition::ptAbsolute);
        }
        this->topLeft->setCoords(boundingRectangle.topLeft());
        this->bottomRight->setCoords(boundingRectangle.bottomRight());
    }

    inline EllipseItem(QCustomPlot* plot, const QRectF& boundingRectangle, const QPen& pen,
                      const QBrush& brush, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
            : EllipseItem { plot, boundingRectangle, movable, coordinates }
    {
        this->setPen(pen);
        this->setBrush(brush);
    }

    virtual ~EllipseItem();

    virtual void move(double dx, double dy) override;

    inline QRectF boundingRectangle() const noexcept { return QRectF(topLeft->coords(), bottomRight->coords()); }
    inline QPointF position() const noexcept { return boundingRectangle().center(); }

    inline void setPos(const QPointF& pos) noexcept
    {
        QRectF rect = boundingRectangle();
        rect.moveCenter(pos);
        topLeft->setCoords(rect.topLeft());
        bottomRight->setCoords(rect.bottomRight());
        replot();
    }

};

/*!
 * \brief The SciQLopPixmapItem class
 */
class SciQLopEllipseItem : public QObject
{
    Q_OBJECT

    QPointer<EllipseItem> m_item;

public:
    /*!
     * \brief SciQLopEllipseItem
     * \param plot The plot to which the item will be added
     * \param boundingRectangle The rectangle that contains the elipse
     * \param movable If the elipse can be moved by the user
     * \param coordinates The coordinates system in which the rectangle is defined (Pixels or Data)
     */
    SciQLopEllipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
    {
        m_item = new EllipseItem(plot->qcp_plot(), boundingRectangle, movable, coordinates);
    }

    SciQLopEllipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, const QPen& pen,
                      const QBrush& brush, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
    {
        m_item
            = new EllipseItem(plot->qcp_plot(), boundingRectangle, pen, brush, movable, coordinates);
    }

    SciQLopEllipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, const QColor& lineColor,
                      qreal lineWidth, const QColor& fillColor, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
    {
        m_item = new EllipseItem(plot->qcp_plot(), boundingRectangle, QPen(lineColor, lineWidth),
                                QBrush(fillColor), movable, coordinates);
    }

    ~SciQLopEllipseItem() = default;

    QPen pen() const noexcept { return m_item->pen(); }

    QBrush brush() const noexcept { return m_item->brush(); }

    inline void setPen(const QPen& pen) noexcept { m_item->setPen(pen); }

    inline void setBrush(const QBrush& brush) noexcept { m_item->setBrush(brush); }

    inline QPointF position() const noexcept { return m_item->position(); }
    inline void setPos(const QPointF& pos) noexcept { m_item->setPos(pos); }
    inline void setPos(double x, double y) noexcept { m_item->setPos(QPointF(x, y)); }
    inline QRectF boundingRectangle() const noexcept { return m_item->boundingRectangle(); }
};

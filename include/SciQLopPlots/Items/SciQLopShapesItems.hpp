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

class ElipseItem : public SciQLopPlotItem<QCPItemEllipse>, public SciQlopItemWithToolTip
{
    Q_OBJECT

public:
#ifndef BINDINGS_H
    Q_SIGNAL void moved(double new_x, double new_y);
#endif // !BINDINGS_H

    inline ElipseItem(QCustomPlot* plot, const QRectF& boundingRectangle, bool movable = false,
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

    inline ElipseItem(QCustomPlot* plot, const QRectF& boundingRectangle, const QPen& pen,
                      const QBrush& brush, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
            : ElipseItem { plot, boundingRectangle, movable, coordinates }
    {
        this->setPen(pen);
        this->setBrush(brush);
    }

    virtual ~ElipseItem();

    virtual void move(double dx, double dy) override;
};

/*!
 * \brief The SciQLopPixmapItem class
 */
class SciQLopElipseItem : public QObject
{
    Q_OBJECT

    QPointer<ElipseItem> m_item;

public:
    /*!
     * \brief SciQLopElipseItem
     * \param plot The plot to which the item will be added
     * \param boundingRectangle The rectangle that contains the elipse
     * \param movable If the elipse can be moved by the user
     * \param coordinates The coordinates system in which the rectangle is defined (Pixels or Data)
     */
    SciQLopElipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
    {
        m_item = new ElipseItem(plot->qcp_plot(), boundingRectangle, movable, coordinates);
    }

    SciQLopElipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, const QPen& pen,
                      const QBrush& brush, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
    {
        m_item
            = new ElipseItem(plot->qcp_plot(), boundingRectangle, pen, brush, movable, coordinates);
    }

    SciQLopElipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, const QColor& lineColor,
                      qreal lineWidth, const QColor& fillColor, bool movable = false,
                      Coordinates coordinates = Coordinates::Pixels)
    {
        m_item = new ElipseItem(plot->qcp_plot(), boundingRectangle, QPen(lineColor, lineWidth),
                                QBrush(fillColor), movable, coordinates);
    }

    ~SciQLopElipseItem() = default;

    QPen pen() const noexcept { return m_item->pen(); }

    QBrush brush() const noexcept { return m_item->brush(); }

    inline void setPen(const QPen& pen) noexcept { m_item->setPen(pen); }

    inline void setBrush(const QBrush& brush) noexcept { m_item->setBrush(brush); }
};

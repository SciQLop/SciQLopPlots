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

#include "SciQLopPlotItem.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/enums.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>

class PixmapItem :public SciQLopPlotItem<QCPItemPixmap>, public SciQlopItemWithToolTip
{
    Q_OBJECT

public:
#ifndef BINDINGS_H
    Q_SIGNAL void moved(double new_position);
#endif // !BINDINGS_H

    inline PixmapItem(QCustomPlot* plot, const QPixmap& pixmap, const QRectF& rect, bool movable = false, Coordinates coordinates = Coordinates::Pixels)
        : SciQLopPlotItem<QCPItemPixmap> { plot }
    {
        this->setPixmap(pixmap);
        this->topLeft->setCoords(rect.topLeft());
        this->bottomRight->setCoords(rect.bottomRight());
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
    }
    virtual ~PixmapItem() { }

    virtual void move(double dx, double dy) override;

};

/*!
 * \brief The SciQLopPixmapItem class
 */
class SciQLopPixmapItem : public QObject
{
    Q_OBJECT

    QPointer<PixmapItem> m_item;
    public:
    /*!
     * \brief SciQLopPixmapItem
     * \param plot The plot to which the item will be added
     * \param pixmap The pixmap to display
     * \param rect The rectangle in which to display the pixmap
     * \param movable If the pixmap can be moved by the user
     * \param coordinates The coordinates system in which the rectangle is defined (Pixels or Data)
     */
    SciQLopPixmapItem(SciQLopPlot* plot, const QPixmap& pixmap, const QRectF& rect, bool movable = false, Coordinates coordinates = Coordinates::Pixels)
    {
        m_item = new PixmapItem(plot->qcp_plot(), pixmap, rect, movable, coordinates);
    }
};


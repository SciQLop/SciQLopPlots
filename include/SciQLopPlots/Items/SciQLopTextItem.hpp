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
#include "SciQLopPlots/qcp_enums.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>


class TextItem : public SciQLopPlotItem<QCPItemText>
{
    Q_OBJECT

public:
#ifndef BINDINGS_H
    Q_SIGNAL void moved(double new_x, double new_y);
#endif // !BINDINGS_H

    inline TextItem(QCustomPlot* plot, const QString& text, const QPointF& position,
                     bool movable = false,
                       Coordinates coordinates = Coordinates::Pixels)
            : SciQLopPlotItem<QCPItemText> { plot }
    {
        this->setMovable(movable);
        if (coordinates == Coordinates::Data)
        {
            QCPItemText::position->setType(QCPItemPosition::ptPlotCoords);
        }
        else
        {
            QCPItemText::position->setType(QCPItemPosition::ptAbsolute);
        }
        QCPItemText::position->setCoords(position);
        QCPItemText::setText(text);
    }



    virtual ~TextItem();

    virtual void move(double dx, double dy) override;



    inline QPointF position() const noexcept { return QCPItemText::position->coords(); }

    inline void setPosition(const QPointF& pos) noexcept
    {
        QCPItemText::position->setCoords(pos);
        replot();
    }
};


class SciQLopTextItem : public QObject
{
    Q_OBJECT

    QPointer<TextItem> m_item;

public:

    SciQLopTextItem(SciQLopPlot* plot, const QString& text, const QPointF& position,
                    bool movable = false,
                    Coordinates coordinates = Coordinates::Pixels)
    {
        if(plot==nullptr)
            throw std::runtime_error("Plot is null");
        else
            m_item = new TextItem(plot->qcp_plot(), text, position, movable, coordinates);
    }

    virtual ~SciQLopTextItem() { }

    void set_position(const QPointF& pos)
    {
        m_item->setPosition(pos);
    }

    QPointF position() const
    {
        return m_item->position();
    }

    void set_text(const QString& text)
    {
        m_item->setText(text);
    }

    QString text() const
    {
        return m_item->text();
    }

};

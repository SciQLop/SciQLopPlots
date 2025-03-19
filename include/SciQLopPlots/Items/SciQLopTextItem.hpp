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
#include "SciQLopPlots/helpers.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>

namespace impl
{
class TextItem : public SciQLopPlotItem<QCPItemText>
{
    Q_OBJECT

public:



    inline TextItem(QCustomPlot* plot, const QString& text, const QPointF& position,
                    bool movable = false, Coordinates coordinates = Coordinates::Pixels)
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

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void moved(double new_x, double new_y);
};
}

class SciQLopTextItem : public SciQLopBoundingRectItemInterface
{
    Q_OBJECT

    QPointer<impl::TextItem> m_item;

public:
    SciQLopTextItem(SciQLopPlot* plot, const QString& text, const QPointF& position,
                    bool movable = false, Coordinates coordinates = Coordinates::Pixels)
    {
        if (plot == nullptr)
            throw std::runtime_error("Plot is null");
        else
            m_item = new impl::TextItem(plot->qcp_plot(), text, position, movable, coordinates);
    }

    virtual ~SciQLopTextItem() { }

    virtual inline void set_position(const QPointF& pos) noexcept override
    {
        qptr_apply(m_item, [&pos](auto& item) { item->setPosition(pos); });
    }

    [[nodiscard]] virtual inline QPointF position() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto& item) { return item->position(); });
    }

    void set_text(const QString& text)
    {
        qptr_apply(m_item, [&text](auto& item) { item->setText(text); });
    }

    QString text() const
    {
        return qptr_apply_or(m_item, [](auto& item) { return item->text(); });
    }
};

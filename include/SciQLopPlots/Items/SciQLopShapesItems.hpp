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
#include "SciQLopPlots/helpers.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>



class EllipseItem : public impl::SciQLopPlotItem<QCPItemEllipse>,
                    public impl::SciQlopItemWithToolTip
{
    Q_OBJECT

public:

    inline EllipseItem(QCustomPlot* plot, const QRectF& boundingRectangle, bool movable = false,
                       Coordinates coordinates = Coordinates::Pixels, const QString& toolTip = "")
            : impl::SciQLopPlotItem<QCPItemEllipse> { plot }
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
        this->setToolTip(toolTip);
    }

    inline EllipseItem(QCustomPlot* plot, const QRectF& boundingRectangle, const QPen& pen,
                       const QBrush& brush, bool movable = false,
                       Coordinates coordinates = Coordinates::Pixels, const QString& toolTip = "")
            : EllipseItem { plot, boundingRectangle, movable, coordinates }
    {
        this->setPen(pen);
        this->setBrush(brush);
    }

    virtual ~EllipseItem();

    virtual void move(double dx, double dy) override;

    inline QRectF boundingRectangle() const noexcept
    {
        return QRectF(topLeft->coords(), bottomRight->coords());
    }

    inline QPointF position() const noexcept { return boundingRectangle().center(); }

    inline void setPosition(const QPointF& pos) noexcept
    {
        QRectF rect = boundingRectangle();
        rect.moveCenter(pos);
        topLeft->setCoords(rect.topLeft());
        bottomRight->setCoords(rect.bottomRight());
        replot();
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void moved(double new_x, double new_y);

};

class CurvedLineItem : public impl::SciQLopPlotItem<QCPItemCurve>,
                       public impl::SciQlopItemWithToolTip
{
    Q_OBJECT

public:
    inline CurvedLineItem(QCustomPlot* plot, const QPointF& start, const QPointF& stop,
                          LineTermination startTerminator = LineTermination::NoneTermination,
                          LineTermination stopTerminator = LineTermination::Arrow,
                          Coordinates coordinates = Coordinates::Pixels,
                          const QString& toolTip = "")
            : SciQLopPlotItem<QCPItemCurve> { plot }
    {
        this->setMovable(false);
        if (coordinates == Coordinates::Data)
        {
            this->start->setType(QCPItemPosition::ptPlotCoords);
            this->end->setType(QCPItemPosition::ptPlotCoords);
        }
        else
        {
            this->start->setType(QCPItemPosition::ptAbsolute);
            this->end->setType(QCPItemPosition::ptAbsolute);
        }
        this->start->setCoords(start);
        this->end->setCoords(stop);
        // from QCP doc : The head corresponds to the end position. ¯\_(ツ)_/¯
        this->setTail(to_qcp(startTerminator));
        this->setHead(to_qcp(stopTerminator));
        this->setToolTip(toolTip);
    }

    virtual ~CurvedLineItem() = default;

    virtual void move(double dx, double dy) override;

    [[nodiscard]] inline QPointF startPos() const noexcept { return start->coords(); }

    [[nodiscard]] inline QPointF stopPos() const noexcept { return end->coords(); }

    inline void setStartPos(const QPointF& pos) noexcept { start->setCoords(pos); }

    inline void setStopPos(const QPointF& pos) noexcept { end->setCoords(pos); }

    [[nodiscard]] inline LineTermination startTermination() const noexcept
    {
        return from_qcp(tail().style());
    }

    [[nodiscard]] inline LineTermination stopTermination() const noexcept
    {
        return from_qcp(head().style());
    }

    inline void setStartTermination(LineTermination termination) noexcept
    {
        setTail(to_qcp(termination));
    }

    inline void setStopTermination(LineTermination termination) noexcept
    {
        setHead(to_qcp(termination));
    }

    [[nodiscard]] inline QPointF startDirPos() const noexcept { return startDir->coords(); }

    [[nodiscard]] inline QPointF stopDirPos() const noexcept { return endDir->coords(); }

    inline void setStartDirPos(const QPointF& pos) noexcept { startDir->setCoords(pos); }

    inline void setStopDirPos(const QPointF& pos) noexcept { endDir->setCoords(pos); }

    [[nodiscard]] QColor color() const noexcept { return pen().color(); }

    [[nodiscard]] qreal width() const noexcept { return pen().widthF(); }

    inline void setColor(const QColor& color) noexcept { setPen(QPen(color, width())); }

    inline void setWidth(qreal width) noexcept { setPen(QPen(color(), width)); }
};

/*!
 * \brief The SciQLopPixmapItem class
 */
class SciQLopEllipseItem : public SciQLopPolygonItemInterface
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
                       Coordinates coordinates = Coordinates::Pixels, const QString& toolTip = "")
            : SciQLopPolygonItemInterface { plot }
    {
        if (plot == nullptr)
            throw std::invalid_argument("plot is nullptr");
        else
            m_item = new EllipseItem(plot->qcp_plot(), boundingRectangle, movable, coordinates,
                                     toolTip);
    }

    SciQLopEllipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, const QPen& pen,
                       const QBrush& brush, bool movable = false,
                       Coordinates coordinates = Coordinates::Pixels, const QString& toolTip = "")
            : SciQLopPolygonItemInterface { plot }
    {
        if (plot == nullptr)
            throw std::invalid_argument("plot is nullptr");
        else
            m_item = new EllipseItem(plot->qcp_plot(), boundingRectangle, pen, brush, movable,
                                     coordinates, toolTip);
    }

    SciQLopEllipseItem(SciQLopPlot* plot, const QRectF& boundingRectangle, const QColor& lineColor,
                       qreal lineWidth, const QColor& fillColor, bool movable = false,
                       Coordinates coordinates = Coordinates::Pixels, const QString& toolTip = "")
            : SciQLopPolygonItemInterface { plot }
    {
        if (plot == nullptr)
            throw std::invalid_argument("plot is nullptr");
        else
            m_item
                = new EllipseItem(plot->qcp_plot(), boundingRectangle, QPen(lineColor, lineWidth),
                                  QBrush(fillColor), movable, coordinates, toolTip);
    }

    ~SciQLopEllipseItem() = default;

    [[nodiscard]] virtual inline QPen pen() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->pen(); });
    }

    [[nodiscard]] virtual inline QBrush brush() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->brush(); });
    }

    inline virtual void set_pen(const QPen& pen) noexcept override
    {
        qptr_apply(m_item, [&pen](auto&& item) { item->setPen(pen); });
    }

    inline virtual void set_brush(const QBrush& brush) noexcept override
    {
        qptr_apply(m_item, [&brush](auto&& item) { item->setBrush(brush); });
    }

    [[nodiscard]] inline QPointF position() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->position(); });
    }

    inline void set_position(const QPointF& pos) noexcept override
    {
        qptr_apply(m_item, [&pos](auto&& item) { item->setPosition(pos); });
    }

    [[nodiscard]] inline QRectF bounding_rectangle() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->boundingRectangle(); });
    }

    [[nodiscard]] inline QString tool_tip() const noexcept override { return m_item->tooltip(); }

    inline void set_tool_tip(const QString& toolTip) noexcept override
    {
        qptr_apply(m_item, [&toolTip](auto&& item) { item->setToolTip(toolTip); });
    }
};

class SciQLopCurvedLineItem : public SciQLopLineItemInterface
{
    Q_OBJECT

    QPointer<CurvedLineItem> m_item;

public:
    SciQLopCurvedLineItem(SciQLopPlot* plot, const QPointF& start, const QPointF& stop,
                          LineTermination startTerminator = LineTermination::NoneTermination,
                          LineTermination stopTerminator = LineTermination::Arrow,
                          Coordinates coordinates = Coordinates::Pixels,
                          const QString& toolTip = "")
            : SciQLopLineItemInterface { plot }
    {
        if (plot == nullptr)
            throw std::invalid_argument("plot is nullptr");
        else
            m_item = new CurvedLineItem(plot->qcp_plot(), start, stop, startTerminator,
                                        stopTerminator, coordinates, toolTip);
    }

    ~SciQLopCurvedLineItem() = default;

    [[nodiscard]] inline QPointF start_position() const noexcept
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->startPos(); });
    }

    [[nodiscard]] inline QPointF stop_position() const noexcept
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->stopPos(); });
    }

    inline void set_start_position(const QPointF& pos) noexcept
    {
        qptr_apply(m_item, [&pos](auto&& item) { item->setStartPos(pos); });
    }

    inline void set_stop_position(const QPointF& pos) noexcept { m_item->setStopPos(pos); }

    [[nodiscard]] inline LineTermination start_termination() const noexcept
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->startTermination(); });
    }

    [[nodiscard]] inline LineTermination stop_termination() const noexcept
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->stopTermination(); });
    }

    inline void set_start_termination(LineTermination termination) noexcept
    {
        qptr_apply(m_item, [&termination](auto&& item) { item->setStartTermination(termination); });
    }

    inline void set_stop_termination(LineTermination termination) noexcept
    {
        qptr_apply(m_item, [&termination](auto&& item) { item->setStopTermination(termination); });
    }

    [[nodiscard]] inline QPointF start_dir_position() const noexcept
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->startDirPos(); });
    }

    [[nodiscard]] inline QPointF stop_dir_position() const noexcept
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->stopDirPos(); });
    }

    inline void set_start_dir_position(const QPointF& pos) noexcept
    {
        qptr_apply(m_item, [&pos](auto&& item) { item->setStartDirPos(pos); });
    }

    inline void set_stop_dir_position(const QPointF& pos) noexcept
    {
        qptr_apply(m_item, [&pos](auto&& item) { item->setStopDirPos(pos); });
    }
};

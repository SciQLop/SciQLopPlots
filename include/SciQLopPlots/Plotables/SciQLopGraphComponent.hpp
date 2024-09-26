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

#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"
#include <qcustomplot.h>

inline QCPScatterStyle::ScatterShape _to_qcp_scatter_shape(GraphMarkerShape marker)
{
    switch (marker)
    {
        case GraphMarkerShape::Circle:
            return QCPScatterStyle::ScatterShape::ssCircle;
        case GraphMarkerShape::Square:
            return QCPScatterStyle::ScatterShape::ssSquare;
        case GraphMarkerShape::Triangle:
            return QCPScatterStyle::ScatterShape::ssTriangle;
        case GraphMarkerShape::Diamond:
            return QCPScatterStyle::ScatterShape::ssDiamond;
        case GraphMarkerShape::Star:
            return QCPScatterStyle::ScatterShape::ssStar;
        case GraphMarkerShape::Plus:
            return QCPScatterStyle::ScatterShape::ssPlus;
        default:
            return QCPScatterStyle::ScatterShape::ssNone;
    }
}

inline GraphMarkerShape _from_qcp_scatter_style(QCPScatterStyle::ScatterShape shape)
{
    switch (shape)
    {
        case QCPScatterStyle::ssCircle:
            return GraphMarkerShape::Circle;
        case QCPScatterStyle::ssSquare:
            return GraphMarkerShape::Square;
        case QCPScatterStyle::ssTriangle:
            return GraphMarkerShape::Triangle;
        case QCPScatterStyle::ssDiamond:
            return GraphMarkerShape::Diamond;
        case QCPScatterStyle::ssStar:
            return GraphMarkerShape::Star;
        case QCPScatterStyle::ssPlus:
            return GraphMarkerShape::Plus;
        default:
            return GraphMarkerShape::NoMarker;
    }
}

class SciQLopGraphComponent : public SciQLopGraphComponentInterface
{
    using graph_or_curve = std::variant<QCPGraph*, QCPCurve*, std::monostate>;

    template <class... Ts>
    struct visitor : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    visitor(Ts...) -> visitor<Ts...>;

    Q_OBJECT
    QPointer<QCPAbstractPlottable> m_plottable;

    inline QCustomPlot* _plot() const
    {
        if (m_plottable)
            return m_plottable->parentPlot();
        return nullptr;
    }

    inline graph_or_curve to_variant() const
    {
        if (auto graph = qobject_cast<QCPGraph*>(m_plottable); graph != nullptr)
            return graph;
        else if (auto curve = qobject_cast<QCPCurve*>(m_plottable); curve != nullptr)
            return curve;
        return std::monostate();
    }

    inline QCPPlottableLegendItem* _legend_item()
    {
        if (m_plottable)
        {
            auto plot = _plot();
            return plot->legend->itemWithPlottable(m_plottable.data());
        }
        return nullptr;
    }

    bool m_selected = false;

public:
    SciQLopGraphComponent(QCPAbstractPlottable* plottable, QObject* parent = nullptr);
    ~SciQLopGraphComponent();

    inline QCPAbstractPlottable* plottable() const noexcept { return m_plottable; }

    inline virtual void set_pen(const QPen& pen) noexcept override
    {
        if (m_plottable)
            m_plottable->setPen(pen);
    }

    inline virtual void set_color(const QColor& color) noexcept override
    {
        if (m_plottable)
        {
            auto pen = m_plottable->pen();
            pen.setColor(color);
            m_plottable->setPen(pen);
        }
    }

    inline virtual void set_marker_shape(GraphMarkerShape marker) noexcept override
    {
        if (m_plottable)
        {
            auto qcp_shape = _to_qcp_scatter_shape(marker);
            std::visit(visitor { [qcp_shape](auto any)
                                 {
                                     auto scatterStyle = any->scatterStyle();
                                     scatterStyle.setShape(qcp_shape);
                                     any->setScatterStyle(scatterStyle);
                                 },
                                 [](std::monostate) {} },
                       to_variant());
        }
    }

    inline virtual void set_marker_pen(const QPen& pen) noexcept override
    {
        if (m_plottable)
        {
            auto qcp_pen = pen;
            std::visit(visitor { [qcp_pen](auto any)
                                 {
                                     auto scatterStyle = any->scatterStyle();
                                     scatterStyle.setPen(qcp_pen);
                                     any->setScatterStyle(scatterStyle);
                                 },
                                 [](std::monostate) {} },
                       to_variant());
        }
    }

    virtual void set_line_width(const qreal width) noexcept override;

    virtual void set_visible(bool visible) noexcept override;

    virtual void set_name(const QString& name) noexcept override;

    virtual void set_selected(bool selected) noexcept override;

    virtual QColor color() const noexcept override;

    inline virtual GraphMarkerShape marker_shape() const noexcept override
    {
        if (m_plottable)
        {
            std::visit(visitor { [](auto any)
                                 { return _from_qcp_scatter_style(any->scatterStyle().shape()); },
                                 [](std::monostate) { return GraphMarkerShape::NoMarker; } },
                       to_variant());
        }
        return GraphMarkerShape::NoMarker;
    }

    inline virtual QPen marker_pen() const noexcept override
    {
        if (m_plottable)
        {
            std::visit(visitor { [](auto any) { return any->scatterStyle().pen(); },
                                 [](std::monostate) { return QPen(); } },
                       to_variant());
        }
        return QPen();
    }

    inline virtual qreal line_width() const noexcept override
    {
        if (m_plottable)
            return m_plottable->pen().widthF();
        return 0;
    }

    inline virtual bool visible() const noexcept override
    {
        if (m_plottable)
            return m_plottable->visible();
        return false;
    }

    inline virtual QString name() const noexcept override { return this->objectName(); }

    inline virtual bool selected() const noexcept override { return m_selected; }
};

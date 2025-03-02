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
#include "SciQLopPlots/qcp_enums.hpp"


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

    virtual void set_pen(const QPen& pen) noexcept override;

    inline virtual void set_color(const QColor& color) noexcept override
    {
        if (m_plottable)
        {
            auto pen = this->pen();
            pen.setColor(color);
            this->set_pen(pen);
        }
    }

    inline virtual void set_line_style(GraphLineStyle style) noexcept override
    {
        if (m_plottable)
        {
            std::visit(visitor { [style](QCPGraph* any)
                                 { any->setLineStyle(to_qcp(style)); },
                                 [style](QCPCurve* any)
                                 {
                                     if (style == GraphLineStyle::NoLine)
                                         any->setLineStyle(QCPCurve::lsNone);
                                     else
                                         any->setLineStyle(QCPCurve::lsLine);
                                 },
                                 [](std::monostate) {} },
                       to_variant());
        }
    }

    inline virtual void set_marker_shape(GraphMarkerShape marker) noexcept override
    {
        if (m_plottable)
        {
            std::visit(visitor { [marker](auto any)
                                 {
                                     auto scatterStyle = any->scatterStyle();
                                     scatterStyle.setShape(to_qcp(marker));
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
            std::visit(visitor { [pen](auto any)
                                 {
                                     auto scatterStyle = any->scatterStyle();
                                     scatterStyle.setPen(pen);
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

    inline virtual QPen pen() const noexcept override
    {
        if (m_plottable)
            return m_plottable->pen();
        return QPen();
    }

    virtual QColor color() const noexcept override;

    inline virtual GraphLineStyle line_style() const noexcept override
    {
        if (m_plottable)
        {
            return std::visit(visitor { [](QCPGraph* any)
                                        { return from_qcp(any->lineStyle()); },
                                        [](QCPCurve* any)
                                        {
                                            if (any->lineStyle() == QCPCurve::lsNone)
                                                return GraphLineStyle::NoLine;
                                            return GraphLineStyle::Line;
                                        },
                                        [](std::monostate) { return GraphLineStyle::NoLine; } },
                              to_variant());
        }
        return GraphLineStyle::NoLine;
    }

    inline virtual GraphMarkerShape marker_shape() const noexcept override
    {
        if (m_plottable)
        {
            return std::visit(
                visitor { [](auto any)
                          { return from_qcp(any->scatterStyle().shape()); },
                          [](std::monostate) { return GraphMarkerShape::NoMarker; } },
                to_variant());
        }
        return GraphMarkerShape::NoMarker;
    }

    inline virtual QPen marker_pen() const noexcept override
    {
        if (m_plottable)
        {
            return std::visit(visitor { [](auto any) { return any->scatterStyle().pen(); },
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

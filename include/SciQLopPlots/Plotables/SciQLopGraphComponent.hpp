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
#include <plottables/plottable-multigraph.h>
#include <plottables/plottable-graph2.h>
#include <layoutelements/layoutelement-legend-group.h>
#include "SciQLopPlots/qcp_enums.hpp"


struct MultiGraphRef {
    QCPMultiGraph* graph = nullptr;
    int componentIndex = -1;
};

class SciQLopGraphComponent : public SciQLopGraphComponentInterface
{
    using graph_or_curve_or_multi = std::variant<QCPGraph*, QCPGraph2*, QCPCurve*, MultiGraphRef, std::monostate>;

    template <class... Ts>
    struct visitor : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    visitor(Ts...) -> visitor<Ts...>;

    Q_OBJECT
    QPointer<QCPAbstractPlottable> m_plottable;
    int m_componentIndex = -1;

    inline QCustomPlot* _plot() const
    {
        if (m_plottable)
            return m_plottable->parentPlot();
        return nullptr;
    }

    inline graph_or_curve_or_multi to_variant() const
    {
        if (m_componentIndex >= 0)
            return MultiGraphRef { qobject_cast<QCPMultiGraph*>(m_plottable.data()),
                                   m_componentIndex };
        if (auto graph = qobject_cast<QCPGraph*>(m_plottable); graph != nullptr)
            return graph;
        else if (auto graph2 = qobject_cast<QCPGraph2*>(m_plottable); graph2 != nullptr)
            return graph2;
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

    inline QCPGroupLegendItem* _group_legend_item()
    {
        auto plot = _plot();
        if (!plot || !plot->legend)
            return nullptr;
        auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
        if (!mg)
            return nullptr;
        for (int i = 0; i < plot->legend->itemCount(); ++i)
            if (auto* gi = qobject_cast<QCPGroupLegendItem*>(plot->legend->item(i)))
                if (gi->multiGraph() == mg)
                    return gi;
        return nullptr;
    }

    bool m_selected = false;

public:
    SciQLopGraphComponent(QCPAbstractPlottable* plottable, QObject* parent = nullptr);
    SciQLopGraphComponent(QCPMultiGraph* multiGraph, int componentIndex,
                          QObject* parent = nullptr);
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
            auto mp = this->marker_pen();
            if (mp.style() == Qt::NoPen)
                mp = QPen(color);
            else
                mp.setColor(color);
            this->set_marker_pen(mp);
        }
    }

    inline virtual void set_line_style(GraphLineStyle style) noexcept override
    {
        if (m_plottable)
        {
            std::visit(visitor { [style](QCPGraph* any)
                                 { any->setLineStyle(to_qcp(style)); },
                                 [style](QCPGraph2* any)
                                 {
                                     any->setLineStyle(
                                         static_cast<QCPGraph2::LineStyle>(
                                             static_cast<int>(to_qcp(style))));
                                 },
                                 [style](QCPCurve* any)
                                 {
                                     if (style == GraphLineStyle::NoLine)
                                         any->setLineStyle(QCPCurve::lsNone);
                                     else
                                         any->setLineStyle(QCPCurve::lsLine);
                                 },
                                 [style](MultiGraphRef ref)
                                 {
                                     ref.graph->setLineStyle(
                                         static_cast<QCPMultiGraph::LineStyle>(
                                             static_cast<int>(to_qcp(style))));
                                 },
                                 [](std::monostate) {} },
                       to_variant());
        }
    }

    inline virtual void set_marker_shape(GraphMarkerShape marker) noexcept override
    {
        if (m_plottable)
        {
            auto set_scatter = [marker](auto* any)
            {
                auto scatterStyle = any->scatterStyle();
                scatterStyle.setShape(to_qcp(marker));
                any->setScatterStyle(scatterStyle);
            };
            std::visit(visitor { [&](QCPGraph* any) { set_scatter(any); },
                                 [&](QCPGraph2* any) { set_scatter(any); },
                                 [&](QCPCurve* any) { set_scatter(any); },
                                 [marker](MultiGraphRef ref)
                                 {
                                     ref.graph->component(ref.componentIndex)
                                         .scatterStyle.setShape(to_qcp(marker));
                                 },
                                 [](std::monostate) {} },
                       to_variant());
        }
    }

    inline virtual void set_marker_pen(const QPen& pen) noexcept override
    {
        if (m_plottable)
        {
            auto set_scatter_pen = [&pen](auto* any)
            {
                auto scatterStyle = any->scatterStyle();
                scatterStyle.setPen(pen);
                any->setScatterStyle(scatterStyle);
            };
            std::visit(visitor { [&](QCPGraph* any) { set_scatter_pen(any); },
                                 [&](QCPGraph2* any) { set_scatter_pen(any); },
                                 [&](QCPCurve* any) { set_scatter_pen(any); },
                                 [pen](MultiGraphRef ref)
                                 {
                                     ref.graph->component(ref.componentIndex)
                                         .scatterStyle.setPen(pen);
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
        {
            if (m_componentIndex >= 0)
            {
                auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
                if (mg)
                    return mg->component(m_componentIndex).pen;
                return QPen();
            }
            return m_plottable->pen();
        }
        return QPen();
    }

    virtual QColor color() const noexcept override;

    inline virtual GraphLineStyle line_style() const noexcept override
    {
        if (m_plottable)
        {
            return std::visit(visitor { [](QCPGraph* any)
                                        { return from_qcp(any->lineStyle()); },
                                        [](QCPGraph2* any)
                                        {
                                            return from_qcp(static_cast<QCPGraph::LineStyle>(
                                                static_cast<int>(any->lineStyle())));
                                        },
                                        [](QCPCurve* any)
                                        {
                                            if (any->lineStyle() == QCPCurve::lsNone)
                                                return GraphLineStyle::NoLine;
                                            return GraphLineStyle::Line;
                                        },
                                        [](MultiGraphRef ref)
                                        {
                                            return from_qcp(static_cast<QCPGraph::LineStyle>(
                                                static_cast<int>(ref.graph->lineStyle())));
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
            auto get_shape = [](auto* any) { return from_qcp(any->scatterStyle().shape()); };
            return std::visit(
                visitor { [&](QCPGraph* any) { return get_shape(any); },
                          [&](QCPGraph2* any) { return get_shape(any); },
                          [&](QCPCurve* any) { return get_shape(any); },
                          [](MultiGraphRef ref)
                          {
                              return from_qcp(
                                  ref.graph->component(ref.componentIndex).scatterStyle.shape());
                          },
                          [](std::monostate) { return GraphMarkerShape::NoMarker; } },
                to_variant());
        }
        return GraphMarkerShape::NoMarker;
    }

    inline virtual QPen marker_pen() const noexcept override
    {
        if (m_plottable)
        {
            auto get_pen = [](auto* any) { return any->scatterStyle().pen(); };
            return std::visit(visitor { [&](QCPGraph* any) { return get_pen(any); },
                                        [&](QCPGraph2* any) { return get_pen(any); },
                                        [&](QCPCurve* any) { return get_pen(any); },
                                        [](MultiGraphRef ref)
                                        {
                                            return ref.graph->component(ref.componentIndex)
                                                .scatterStyle.pen();
                                        },
                                        [](std::monostate) { return QPen(); } },
                              to_variant());
        }
        return QPen();
    }

    inline virtual qreal line_width() const noexcept override
    {
        if (m_plottable)
        {
            if (m_componentIndex >= 0)
            {
                auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
                if (mg)
                    return mg->component(m_componentIndex).pen.widthF();
                return 0;
            }
            return m_plottable->pen().widthF();
        }
        return 0;
    }

    inline virtual bool visible() const noexcept override
    {
        if (m_plottable)
        {
            if (m_componentIndex >= 0)
            {
                auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
                if (mg)
                    return mg->component(m_componentIndex).visible;
                return false;
            }
            return m_plottable->visible();
        }
        return false;
    }

    inline virtual QString name() const noexcept override { return this->objectName(); }

    inline virtual bool selected() const noexcept override { return m_selected; }
};

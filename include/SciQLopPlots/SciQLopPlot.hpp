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
#include <SciQLopPlots/SciQLopColorMap.hpp>

#include <QPointF>
#include <SciQLopPlots/SciQLopCurve.hpp>
#include <SciQLopPlots/SciQLopGraph.hpp>
#include <SciQLopPlots/SciQLopPlotItem.hpp>
#include <SciQLopPlots/SciQLopTracer.hpp>
#include <optional>
#include <qcustomplot.h>

class SciQLopPlot : public QCustomPlot
{
    Q_OBJECT

    double m_scroll_factor = 1.;
    TracerWithToolTip* m_tracer = nullptr;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void scroll_factor_changed(double factor);
#endif
    explicit SciQLopPlot(QWidget* parent = nullptr);

    virtual ~SciQLopPlot() Q_DECL_OVERRIDE;
    QCPColorMap* addColorMap(QCPAxis* x, QCPAxis* y);

    SciQLopGraph* addSciQLopGraph(QCPAxis* x, QCPAxis* y, QStringList labels,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopGraph* addSciQLopGraph(QCPAxis* x, QCPAxis* y,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopCurve* addSciQLopCurve(QCPAxis* x, QCPAxis* y, QStringList labels,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopCurve* addSciQLopCurve(QCPAxis* x, QCPAxis* y,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopColorMap* addSciQLopColorMap(QCPAxis* x, QCPAxis* y, const QString& name,
        SciQLopColorMap::DataOrder dataOrder = SciQLopColorMap::DataOrder::xFirst);

    void set_scroll_factor(double factor) noexcept;
    inline double scroll_factor() const noexcept { return m_scroll_factor; }

    void enable_cursor(bool enable = true) noexcept;

    void enable_legend(bool show = true) noexcept;

    void minimize_margins();

protected:
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;

    virtual void keyPressEvent(QKeyEvent* event) override;

    virtual bool event(QEvent* event) override;

    bool _update_tracer(const QPointF& pos);
    bool _update_mouse_cursor(QMouseEvent* event);
    bool _handle_tool_tip(QEvent* event);
    QCPGraph* _nearest_graph(const QPointF& pos);
    std::optional<std::tuple<double, double>> _nearest_data_point(
        const QPointF& pos, QCPGraph* graph);

    template <typename T, typename... Args>
    T* _new_plottable_wrapper(Args&&... args)
    {
        SQPQCPAbstractPlottableWrapper* plottable = new T(this, std::forward<Args>(args)...);
        _register_plottable_wrapper(plottable);
        return reinterpret_cast<T*>(plottable);
    }

    void _register_plottable_wrapper(SQPQCPAbstractPlottableWrapper* plottable);
    void _register_plottable(QCPAbstractPlottable* plotable);

    QCPAbstractPlottable* plottable(const QString& name) const;

    void _legend_double_clicked(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);

private:
    void _wheel_pan(QCPAxis* axis, const double wheelSteps, const QPointF& pos);
    void _wheel_zoom(QCPAxis* axis, const double wheelSteps, const QPointF& pos);
};

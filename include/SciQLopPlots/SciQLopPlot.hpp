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
#include <SciQLopPlots/Plotables/SciQLopColorMap.hpp>

#include <QPointF>
#include <SciQLopPlots/Items/SciQLopPlotItem.hpp>
#include <SciQLopPlots/Items/SciQLopTracer.hpp>
#include <SciQLopPlots/Plotables/SciQLopCurve.hpp>
#include <SciQLopPlots/Plotables/SciQLopGraph.hpp>
#include <SciQLopPlots/SciQLopPlotAxis.hpp>
#include <optional>
#include <qcustomplot.h>

class SciQLopPlotInterface : public QWidget
{
    Q_OBJECT
protected:
    QList<SciQLopPlotAxisInterface*> m_axes_to_rescale;
    QList<SciQLopPlotAxisInterface*> m_frozen_axes;

public:
    SciQLopPlotInterface(QWidget* parent = nullptr) : QWidget(parent) { }
    virtual ~SciQLopPlotInterface() = default;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept { return nullptr; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept { return nullptr; }
    virtual SciQLopPlotAxisInterface* z_axis() const noexcept { return nullptr; }
    virtual SciQLopPlotAxisInterface* y2_axis() const noexcept { return nullptr; }
    virtual SciQLopPlotAxisInterface* x2_axis() const noexcept { return nullptr; }

    inline virtual SciQLopPlotAxisInterface* axis(Qt::Axis axis) const noexcept
    {
        switch (axis)
        {
            case Qt::XAxis:
                return x_axis();
            case Qt::YAxis:
                return y_axis();
            case Qt::ZAxis:
                return z_axis();
            default:
                return nullptr;
        }
    }

    virtual SciQLopPlotAxisInterface* axis(Qt::AnchorPoint pos, int index = 0) const noexcept
    {
        if (index == 0)
        {

            switch (pos)
            {
                case Qt::AnchorPoint::AnchorBottom:
                    return x_axis();
                case Qt::AnchorPoint::AnchorLeft:
                    return y_axis();
                case Qt::AnchorPoint::AnchorRight:
                    return y2_axis();
                case Qt::AnchorPoint::AnchorTop:
                    return x2_axis();
                default:
                    break;
            }
        }
        return nullptr;
    }


    virtual void set_scroll_factor(double factor) noexcept { }
    virtual double scroll_factor() const noexcept { return std::nan(""); }

    virtual void enable_cursor(bool enable = true) noexcept { }
    virtual void enable_legend(bool show = true) noexcept { }

    virtual void minimize_margins() { }

    virtual void replot(bool immediate = false) { }


    inline virtual void rescale_axes() noexcept { rescale_axes(m_axes_to_rescale); }
    inline virtual void rescale_axes(const QList<SciQLopPlotAxisInterface*>& axes) noexcept
    {
        for (auto ax : axes)
        {
            ax->rescale();
        }
    }

#ifndef BINDINGS_H
    Q_SIGNAL void scroll_factor_changed(double factor);
    Q_SIGNAL void x_axis_range_changed(double lower, double upper);
    Q_SIGNAL void y_axis_range_changed(double lower, double upper);
    Q_SIGNAL void y2_axis_range_changed(double lower, double upper);
#endif
protected:
    inline virtual QList<SciQLopPlotAxisInterface*> axes_to_rescale() const noexcept
    {
        return m_axes_to_rescale;
    }
    inline virtual void set_axes_to_rescale(QList<SciQLopPlotAxisInterface*>&& axes)
    {
        m_axes_to_rescale = axes;
    }

    inline void freeze_axis(SciQLopPlotAxisInterface* axis) noexcept { m_frozen_axes.append(axis); }

    virtual QList<SciQLopPlotAxisInterface*> selected_axes() const noexcept { return {}; }
    virtual SciQLopPlotAxisInterface* axis_at(const QPointF& pos) const noexcept { return nullptr; }

    inline virtual void keyPressEvent(QKeyEvent* event) override
    {
        QList<SciQLopPlotAxisInterface*> axes;
        QList<SciQLopPlotAxisInterface*> axes_to_rescale;
        if (auto ax = axis_at(mapFromGlobal(QCursor::pos())))
            axes.append(ax);
        else
            axes = selected_axes();
        if (axes.isEmpty())
            axes_to_rescale = m_axes_to_rescale;
        else
            axes_to_rescale = axes;

        for (auto ax : m_frozen_axes)
            axes_to_rescale.removeAll(ax);

        switch (event->key())
        {
            case Qt::Key_M:
                rescale_axes(axes_to_rescale);
                event->accept();
                replot();
                break;
            case Qt::Key_L:
                for (auto ax : axes)
                    ax->set_log(not ax->log());
                event->accept();
                replot();
                break;
            default:
                break;
        }
        if (!event->isAccepted())
            QWidget::keyPressEvent(event);
    }
};
namespace _impl
{

class SciQLopPlot : public QCustomPlot
{
    Q_OBJECT

    double m_scroll_factor = 1.;
    TracerWithToolTip* m_tracer = nullptr;
    QCPColorScale* m_color_scale = nullptr;
    QList<SciQLopPlotAxis*> m_axes;


public:
#ifndef BINDINGS_H
    Q_SIGNAL void scroll_factor_changed(double factor);
    Q_SIGNAL void x_axis_range_changed(double lower, double upper);
    Q_SIGNAL void y_axis_range_changed(double lower, double upper);
    Q_SIGNAL void y2_axis_range_changed(double lower, double upper);
#endif
    explicit SciQLopPlot(QWidget* parent = nullptr);

    virtual ~SciQLopPlot() Q_DECL_OVERRIDE;
    QCPColorMap* addColorMap();

    SciQLopGraph* addSciQLopGraph(
        QStringList labels, SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopGraph* addSciQLopGraph(
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopCurve* addSciQLopCurve(
        QStringList labels, SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopCurve* addSciQLopCurve(
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst);

    SciQLopColorMap* addSciQLopColorMap(const QString& name,
        SciQLopColorMap::DataOrder dataOrder = SciQLopColorMap::DataOrder::xFirst,
        bool y_log_scale = false, bool z_log_scale = false);

    inline void set_scroll_factor(double factor) noexcept { m_scroll_factor = factor; }
    inline double scroll_factor() const noexcept { return m_scroll_factor; }

    inline bool has_colormap() { return m_color_scale->visible(); }
    inline QCPColorScale* color_scale() const noexcept { return m_color_scale; }

    void minimize_margins();

    inline int calculateAutoMargin(QCP::MarginSide side)
    {
        return 0; // axisRect()->calculateAutoMargin(side);
    }

    QMargins minimal_axis_margins();

    inline QCPAxis* axis(const QCPAxis::AxisType type) const noexcept
    {
        return axisRect()->axis(type);
    }

    inline SciQLopPlotAxis* axis(int index) const noexcept
    {
        if (index < m_axes.size())
            return m_axes[index];
        return nullptr;
    }

    inline void set_axis_range(const QCPAxis::AxisType type, double lower, double upper)
    {
        auto ax = this->axis(type);
        if (ax->range().lower != lower || ax->range().upper != upper)
        {
            ax->setRange(lower, upper);
            replot(rpQueuedReplot);
        }
    }

    inline void set_axis_visible(const QCPAxis::AxisType type, bool visible)
    {
        auto ax = this->axis(type);
        if (ax->visible() != visible)
        {
            ax->setVisible(visible);
            replot(rpQueuedReplot);
        }
    }

    inline void set_axis_log(QCPAxis* axis, bool log)
    {
        if (log and axis->scaleType() != QCPAxis::stLogarithmic)
        {
            axis->setScaleType(QCPAxis::stLogarithmic);
            axis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTickerLog));
        }
        else if (not log and axis->scaleType() != QCPAxis::stLinear)
        {
            axis->setScaleType(QCPAxis::stLinear);
            axis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
        }
    }

    inline void set_axis_log(const QCPAxis::AxisType type, bool log)
    {
        auto ax = this->axis(type);
        set_axis_log(ax, log);
    }

    inline void set_axis_label(const QCPAxis::AxisType type, const QString& label)
    {
        auto ax = this->axis(type);
        ax->setLabel(label);
    }

    inline std::pair<double, double> axis_range(const QCPAxis::AxisType type) const noexcept
    {
        auto ax = axis(type);
        return { ax->range().lower, ax->range().upper };
    }

    QList<SciQLopPlotAxisInterface*> selected_axes() const noexcept
    {
        QList<SciQLopPlotAxisInterface*> axes;
        for (auto ax : m_axes)
        {
            if (ax->selected())
                axes.append(ax);
        }
        return axes;
    }

    SciQLopPlotAxisInterface* axis_at(const QPointF& pos) const noexcept
    {
        for (auto ax : m_axes)
        {
            if (ax->qcp_axis()->selectTest(pos, false) >= 0)
                return ax;
        }
        return nullptr;
    }


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

    void _wheel_pan(QCPAxis* axis, const double wheelSteps, const QPointF& pos);
    void _wheel_zoom(QCPAxis* axis, const double wheelSteps, const QPointF& pos);

    int _minimal_margin(QCP::MarginSide side);
};
}


class SciQLopPlot : public SciQLopPlotInterface
{
    Q_OBJECT
    _impl::SciQLopPlot* m_impl;

protected:
    virtual QList<SciQLopPlotAxisInterface*> selected_axes() const noexcept override
    {
        return m_impl->selected_axes();
    }

    virtual SciQLopPlotAxisInterface* axis_at(const QPointF& pos) const noexcept override
    {
        return m_impl->axis_at(m_impl->mapFrom(this, pos));
    }

public:
    explicit SciQLopPlot(QWidget* parent = nullptr);
    virtual ~SciQLopPlot() Q_DECL_OVERRIDE;

    inline QCustomPlot* qcp_plot() const noexcept { return m_impl; }

    void set_scroll_factor(double factor) noexcept override;
    double scroll_factor() const noexcept override;

    void enable_cursor(bool enable = true) noexcept override;

    void enable_legend(bool show = true) noexcept override;

    void minimize_margins() override;

    inline bool has_colormap() { return m_impl->has_colormap(); }
    inline QCPColorScale* color_scale() const noexcept { return m_impl->color_scale(); }


    inline int calculateAutoMargin(QCP::MarginSide side)
    {
        return m_impl->calculateAutoMargin(side);
    }

    inline virtual SciQLopPlotAxisInterface* x_axis() const noexcept Q_DECL_OVERRIDE
    {
        return m_impl->axis(0);
    }
    inline virtual SciQLopPlotAxisInterface* y_axis() const noexcept Q_DECL_OVERRIDE
    {
        return m_impl->axis(1);
        ;
    }
    inline virtual SciQLopPlotAxisInterface* z_axis() const noexcept Q_DECL_OVERRIDE
    {
        return nullptr;
    }
    inline virtual SciQLopPlotAxisInterface* x2_axis() const noexcept Q_DECL_OVERRIDE
    {
        return m_impl->axis(2);
    }
    inline virtual SciQLopPlotAxisInterface* y2_axis() const noexcept Q_DECL_OVERRIDE
    {
        return m_impl->axis(3);
    }

    void replot(bool immediate = false) override;

    inline SciQLopGraph* addSciQLopGraph(
        QStringList labels, SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        return m_impl->addSciQLopGraph(labels, dataOrder);
    }

    inline SciQLopGraph* addSciQLopGraph(
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        return m_impl->addSciQLopGraph(dataOrder);
    }

    inline SciQLopCurve* addSciQLopCurve(
        QStringList labels, SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        return m_impl->addSciQLopCurve(labels, dataOrder);
    }

    inline SciQLopCurve* addSciQLopCurve(
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        return m_impl->addSciQLopCurve(dataOrder);
    }

    inline SciQLopColorMap* addSciQLopColorMap(const QString& name,
        SciQLopColorMap::DataOrder dataOrder = SciQLopColorMap::DataOrder::xFirst,
        bool y_log_scale = false, bool z_log_scale = false)
    {
        return m_impl->addSciQLopColorMap(name, dataOrder, y_log_scale, z_log_scale);
    }
};

class SciQLopTimeSeriesPlot : public SciQLopPlot
{
    Q_OBJECT

public:
    explicit SciQLopTimeSeriesPlot(QWidget* parent = nullptr);
    virtual ~SciQLopTimeSeriesPlot() Q_DECL_OVERRIDE = default;
};


inline QList<SciQLopPlot*> only_sciqloplpots(const QList<SciQLopPlotInterface*>& plots)
{
    QList<SciQLopPlot*> filtered;
    for (auto plot : plots)
    {
        if (auto p = dynamic_cast<SciQLopPlot*>(plot))
            filtered.append(p);
    }
    return filtered;
}

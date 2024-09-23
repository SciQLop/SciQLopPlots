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
#include "SciQLopPlots/Inspector/InspectorBase.hpp"
#include "SciQLopPlots/Items/SciQLopPlotItem.hpp"
#include "SciQLopPlots/Items/SciQLopTracer.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"
#include "SciQLopPlots/Plotables/SciQLopCurve.hpp"
#include "SciQLopPlots/Plotables/SciQLopLineGraph.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/enums.hpp"
#include <QPointF>
#include <optional>
#include <qcustomplot.h>

namespace _impl
{

class SciQLopPlot : public QCustomPlot
{
    Q_OBJECT

    QTimer* m_replot_timer;
    double m_scroll_factor = 1.;
    TracerWithToolTip* m_tracer = nullptr;
    QCPColorScale* m_color_scale = nullptr;
    QList<SciQLopPlotAxis*> m_axes;
    bool m_replot_pending = false;

    QList<SQPQCPAbstractPlottableWrapper*> m_plottables;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void scroll_factor_changed(double factor);
    Q_SIGNAL void x_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void x2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void plotables_list_changed();
#endif
    explicit SciQLopPlot(QWidget* parent = nullptr);

    virtual ~SciQLopPlot() Q_DECL_OVERRIDE;
    QCPColorMap* addColorMap();

    template <typename T, typename... Args>
    T* add_plottable(Args&&... args)
    {
        return _new_plottable_wrapper<T>(this->xAxis, this->yAxis, std::forward<Args>(args)...);
    }

    SQPQCPAbstractPlottableWrapper* sqp_plottable(int index = -1);
    SQPQCPAbstractPlottableWrapper* sqp_plottable(const QString& name);
    const QList<SQPQCPAbstractPlottableWrapper*>& sqp_plottables() const;

    SciQLopColorMap* add_color_map(
        const QString& name, bool y_log_scale = false, bool z_log_scale = false);
    SciQLopColorMapFunction* add_color_map(GetDataPyCallable&& callable, const QString& name,
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

    inline SciQLopPlotAxisInterface* axis(int index) const noexcept
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

    void replot(QCustomPlot::RefreshPriority priority = rpQueuedReplot);

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

    void _configure_color_map(SciQLopColorMap* cmap, bool y_log_scale, bool z_log_scale);

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

protected:
    SciQLopPlotDummyAxis* m_time_axis = nullptr;
    _impl::SciQLopPlot* m_impl = nullptr;
    void _connect_callable_sync(SQPQCPAbstractPlottableWrapper* plottable, QObject* sync_with);

    virtual QList<SciQLopPlotAxisInterface*> selected_axes() const noexcept override
    {
        return m_impl->selected_axes();
    }

    virtual SciQLopPlotAxisInterface* axis_at(const QPointF& pos) const noexcept override
    {
        return m_impl->axis_at(m_impl->mapFrom(this, pos));
    }

    void _configure_plotable(SQPQCPAbstractPlottableWrapper* plottable, const QStringList& labels,
        const QList<QColor>& colors);

    virtual SciQLopGraphInterface* plot_impl(const PyBuffer& x, const PyBuffer& y,
        QStringList labels = QStringList(), QList<QColor> colors = QList<QColor>(),

        ::GraphType graph_type = ::GraphType::Line) override;

    virtual SciQLopGraphInterface* plot_impl(const PyBuffer& x, const PyBuffer& y,
        const PyBuffer& z, QString name = QStringLiteral("ColorMap"), bool y_log_scale = false,
        bool z_log_scale = false) override;

    virtual SciQLopGraphInterface* plot_impl(GetDataPyCallable callable,
        QStringList labels = QStringList(), QList<QColor> colors = QList<QColor>(),
        ::GraphType graph_type = ::GraphType::Line, QObject* sync_with = nullptr) override;

    virtual SciQLopGraphInterface* plot_impl(GetDataPyCallable callable,
        QString name = QStringLiteral("ColorMap"), bool y_log_scale = false,
        bool z_log_scale = false, QObject* sync_with = nullptr) override;

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

    inline virtual SciQLopPlotAxisInterface* time_axis() const noexcept override
    {
        return m_time_axis;
    }

    inline virtual SciQLopPlotAxisInterface* x_axis() const noexcept Q_DECL_OVERRIDE
    {
        return m_impl->axis(0);
    }

    inline virtual SciQLopPlotAxisInterface* y_axis() const noexcept Q_DECL_OVERRIDE
    {
        return m_impl->axis(1);
    }

    inline virtual SciQLopPlotAxisInterface* z_axis() const noexcept Q_DECL_OVERRIDE
    {
        return m_impl->axis(4);
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


    virtual SciQLopGraphInterface* graph(int index = -1) override;
    virtual SciQLopGraphInterface* graph(const QString& name) override;
    virtual QList<SciQLopGraphInterface*> graphs() const noexcept override;
};

inline QList<SciQLopPlot*> only_sciqlopplots(const QList<SciQLopPlotInterface*>& plots)
{
    QList<SciQLopPlot*> filtered;
    for (auto plot : plots)
    {
        if (auto p = dynamic_cast<SciQLopPlot*>(plot))
            filtered.append(p);
    }
    return filtered;
}

inline QList<QPointer<SciQLopPlot>> only_sciqlopplots(
    const QList<QPointer<SciQLopPlotInterface>>& plots)
{
    QList<QPointer<SciQLopPlot>> filtered;
    for (auto& plot : plots)
    {
        if (!plot.isNull())
        {
            if (auto p = dynamic_cast<SciQLopPlot*>(plot.data()))
                filtered.append(p);
        }
    }
    return filtered;
}

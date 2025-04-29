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
#include "SciQLopPlots/SciQLopPlotLegend.hpp"
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

    QList<SciQLopPlottableInterface*> m_plottables;
    SciQLopColorMap* m_color_map = nullptr;


public:
#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void scroll_factor_changed(double factor);
    Q_SIGNAL void x_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void x2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void z_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void plotables_list_changed();
    Q_SIGNAL void resized(QSize size);

public:
    explicit SciQLopPlot(QWidget* parent = nullptr);

    virtual ~SciQLopPlot() Q_DECL_OVERRIDE;
    QCPColorMap* addColorMap();

    template <typename T, typename... Args>
    T* add_plottable(Args&&... args)
    {
        return _new_plottable_wrapper<T>(this->m_axes[0], this->m_axes[1],
                                         std::forward<Args>(args)...);
    }

    SciQLopPlottableInterface* sqp_plottable(int index = -1);
    SciQLopPlottableInterface* sqp_plottable(const QString& name);
    const QList<SciQLopPlottableInterface*>& sqp_plottables() const;

    SciQLopColorMap* add_color_map(const QString& name, bool y_log_scale = false,
                                   bool z_log_scale = false);
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

    virtual void resizeEvent(QResizeEvent* event) override;

    bool _update_tracer(const QPointF& pos);
    bool _update_mouse_cursor(QMouseEvent* event);
    bool _handle_tool_tip(QEvent* event);
    QCPGraph* _nearest_graph(const QPointF& pos);
    std::optional<std::tuple<double, double>> _nearest_data_point(const QPointF& pos,
                                                                  QCPGraph* graph);

    template <typename T, typename... Args>
    T* _new_plottable_wrapper(Args&&... args)
    {
        SciQLopGraphInterface* plottable = new T(this, std::forward<Args>(args)...);
        _register_plottable_wrapper(plottable);
        return reinterpret_cast<T*>(plottable);
    }

    void _register_plottable_wrapper(SciQLopPlottableInterface* plottable);
    void _register_plottable(QCPAbstractPlottable* plotable);

    SciQLopGraphInterface* plottable_wrapper(QCPAbstractPlottable* plottable);

    void _ensure_colorscale_is_visible(SciQLopColorMap* cmap);

    QCPAbstractPlottable* plottable(const QString& name) const;

    void _legend_double_clicked(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);

    void _wheel_pan(QCPAxis* axis, const double wheelSteps);
    void _wheel_zoom(QCPAxis* axis, const double wheelSteps, const QPointF& pos);
    void _pinch_zoom(QPinchGesture* gesture);

    int _minimal_margin(QCP::MarginSide side);
};
}

/*!
 * \brief The SciQLopPlot class
 *
 */
class SciQLopPlot : public SciQLopPlotInterface
{
    Q_OBJECT


protected:
    SciQLopPlotDummyAxis* m_time_axis = nullptr;
    _impl::SciQLopPlot* m_impl = nullptr;
    QList<QColor> m_color_palette;
    SciQLopPlotLegend* m_legend = nullptr;
    int m_color_palette_index = 0;
    bool m_auto_scale = false;


    void _configure_color_map(SciQLopColorMapInterface* cmap, bool y_log_scale, bool z_log_scale);

    void _connect_callable_sync(SciQLopPlottableInterface* plottable, QObject* sync_with);

    virtual QList<SciQLopPlotAxisInterface*> selected_axes() const noexcept override
    {
        return m_impl->selected_axes();
    }

    virtual SciQLopPlotAxisInterface* axis_at(const QPointF& pos) const noexcept override
    {
        return m_impl->axis_at(m_impl->mapFrom(this, pos));
    }

    void _configure_plotable(SciQLopGraphInterface* plottable, const QStringList& labels,
                             const QList<QColor>& colors, ::GraphType graph_type,
                             ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker);

    virtual SciQLopGraphInterface*
    plot_impl(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, QVariantMap metaData={}) override;

    virtual SciQLopColorMapInterface* plot_impl(const PyBuffer& x, const PyBuffer& y,
                                                const PyBuffer& z,
                                                QString name = QStringLiteral("ColorMap"),
                                                bool y_log_scale = false,
                                                bool z_log_scale = false,QVariantMap metaData={}) override;

    virtual SciQLopGraphInterface*
    plot_impl(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              QObject* sync_with = nullptr,QVariantMap metaData={}) override;

    virtual SciQLopColorMapInterface* plot_impl(GetDataPyCallable callable,
                                                QString name = QStringLiteral("ColorMap"),
                                                bool y_log_scale = false, bool z_log_scale = false,
                                                QObject* sync_with = nullptr,QVariantMap metaData={}) override;

    virtual void toggle_selected_objects_visibility() noexcept override;

public:
    explicit SciQLopPlot(QWidget* parent = nullptr);
    virtual ~SciQLopPlot() Q_DECL_OVERRIDE;

    inline QCustomPlot* qcp_plot() const noexcept { return m_impl; }

    /*!
     * \brief set_scroll_factor Set the scroll factor of the plot.
     * \param factor The new scroll factor.
     * \note The scroll factor is used to adjust the scroll speed of the plot.
     *
     * \sa scroll_factor
     */
    void set_scroll_factor(double factor) noexcept override;

    /*!
     * \brief scroll_factor Get the scroll factor of the plot.
     * \return The scroll factor.
     * \note The scroll factor is used to adjust the scroll speed of the plot.
     *
     * \sa set_scroll_factor
     */
    double scroll_factor() const noexcept override;

    /*!
     * \brief enable_cursor Enable or disable the cursor.
     * \param enable True to enable the cursor, false to disable it.
     * \note The cursor is a round marker that follows the mouse cursor on the plot and displays the
     * corresponding graph values.
     */
    void enable_cursor(bool enable = true) noexcept override;

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

    inline virtual SciQLopPlotLegendInterface* legend() const noexcept override { return m_legend; }

    void replot(bool immediate = false) override;


    virtual SciQLopPlottableInterface* plottable(int index = -1) override;
    virtual SciQLopPlottableInterface* plottable(const QString& name) override;
    virtual QList<SciQLopPlottableInterface*> plottables() const noexcept override;

    inline virtual QList<QColor> color_palette() const noexcept override { return m_color_palette; }

    inline virtual void set_color_palette(const QList<QColor>& palette) noexcept override
    {
        m_color_palette = palette;
    }

    inline virtual void set_auto_scale(bool auto_scale) noexcept override
    {
        m_auto_scale = auto_scale;
        Q_EMIT auto_scale_changed(auto_scale);
    }
    inline virtual bool auto_scale() const noexcept override { return m_auto_scale; }
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

inline QList<QPointer<SciQLopPlot>>
only_sciqlopplots(const QList<QPointer<SciQLopPlotInterface>>& plots)
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

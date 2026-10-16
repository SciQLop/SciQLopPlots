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


#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include <limits>

class ColorScaleController;
class SciQLopNDProjectionCurves;

//! A pane of a projection plot: a plot the projection owns, so the inspector neither deletes
//! nor moves it.
class SciQLopNDProjectionPane : public SciQLopPlot
{
    Q_OBJECT
public:
    using SciQLopPlot::SciQLopPlot;
    using SciQLopPlot::rescale_axes;

    //! With SameScale, a pane can't fit alone: the projection fits all its panes together.
    void rescale_axes(const QList<SciQLopPlotAxisInterface*>& axes) noexcept override;
    void fit_own_axes() noexcept { SciQLopPlot::rescale_axes({ x_axis(), y_axis() }); }
};

class SciQLopNDProjectionPlot : public SciQLopPlotInterface, public SciQLopExportable
{
    Q_OBJECT

protected:
    SciQLopPlotDummyAxis* m_time_axis = nullptr;
    QList<QColor> m_color_palette;
    int m_color_palette_index = 0;
    QList<SciQLopPlot*> m_plots;
    ProjectionAxisLinking m_axis_linking = ProjectionAxisLinking::ByDimension;
    QList<QMetaObject::Connection> m_same_scale_connections;
    bool m_equalizing_spans = false;
    QList<QPair<int, int>> m_dimension_pairs;
    QStringList m_dimension_names;
    bool m_equal_aspect_ratio = false;
    bool m_linked_crosshairs = false;
    bool m_time_color_enabled = false;
    bool m_enforcing_aspect = false;
    bool m_shared_legend = false;
    ColorScaleController* m_scale = nullptr;
    QList<QCPItemTracer*> m_time_markers;
    double m_time_marker_key = std::numeric_limits<double>::quiet_NaN();
    QPointer<SciQLopTheme> m_theme;

    Q_SLOT void _enforce_equal_aspect();
    void _ensure_marker_layer();
    void _place_time_markers();
    SciQLopGraphInterface* _adopt(SciQLopGraphInterface* graph);
    void _setup_color_scale();
    void _link_axes(bool linked);
    void _set_same_scale(bool enabled);
    void _equalize_spans(const SciQLopPlotAxisInterface* source);
    QList<SciQLopPlotAxisInterface*> _pane_axes() const;
    void _apply_axis_labels();

    virtual SciQLopGraphInterface*
    plot_impl(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              QObject* sync_with = nullptr,QVariantMap metaData={}) override;

    virtual SciQLopGraphInterface*
    plot_impl(const QList<SciQLopPyBuffer>& data, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(),
              ::GraphType graph_type = ::GraphType::ParametricCurve,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,QVariantMap metaData={}) override;


public:
    SciQLopNDProjectionPlot(std::size_t projection_count = 3, QWidget* parent = nullptr);
    virtual ~SciQLopNDProjectionPlot() Q_DECL_OVERRIDE = default;

    int subplot_count() const noexcept { return m_plots.size(); }

    SciQLopPlot* subplot(int index) const noexcept
    {
        if (index < 0 || index >= m_plots.size())
            return nullptr;
        return m_plots[index];
    }

    void set_axis_labels(const QStringList& dimension_names) noexcept;

    /*!
     * \brief set_dimension_pairs Which two dimensions each subplot shows, as (x, y)
     *        indices: one pair per subplot. The default is cyclic, (i, i + 1 mod n):
     *        XY, YZ, ZX for three. [(0, 1), (0, 2), (1, 2)] gives XY, XZ, YZ.
     *        Axes showing the same dimension stay linked. Graphs added before the
     *        change keep their pairs until their next data.
     * \throws std::invalid_argument unless there is one pair per subplot, of two
     *         different dimensions below the subplot count.
     */
    void set_dimension_pairs(const QList<QPair<int, int>>& pairs);
    inline QList<QPair<int, int>> dimension_pairs() const noexcept { return m_dimension_pairs; }

    using SciQLopPlotInterface::rescale_axes;
    //! Fits every pane to its data; with SameScale every axis then takes the widest span.
    void rescale_axes() noexcept override;

    void set_axis_linking(ProjectionAxisLinking linking) noexcept;
    inline ProjectionAxisLinking axis_linking() const noexcept { return m_axis_linking; }

    //! true: ByDimension, false: Independent.
    void set_linked_axes(bool linked) noexcept;
    inline bool linked_axes() const noexcept
    {
        return m_axis_linking != ProjectionAxisLinking::Independent;
    }

    //! Square panes. Always on with SameScale, which needs them for a circle to stay round.
    void set_equal_aspect_ratio(bool enabled) noexcept;
    bool equal_aspect_ratio() const noexcept { return m_equal_aspect_ratio; }

    virtual SciQLopPlottableInterface* plottable(int index = -1) override;
    virtual SciQLopPlottableInterface* plottable(const QString& name) override;
    virtual QList<SciQLopPlottableInterface*> plottables() const noexcept override;

    inline virtual QList<QColor> color_palette() const noexcept override { return m_color_palette; }

    inline virtual void set_color_palette(const QList<QColor>& palette) noexcept override
    {
        m_color_palette = palette;
        for (auto* plot : m_plots)
            plot->set_color_palette(palette);
    }

    /*!
     * \brief add_reference_curve Add a static curve, projected on every subplot.
     * \param dimensions Either one buffer per subplot, or -- as parametric_curve()
     *        takes it -- time first followed by one buffer per subplot. Only the
     *        latter carries time, so only it can be time-coloured or time-marked.
     * \return The new curve, or nullptr on any other buffer count.
     */
    SciQLopGraphInterface* add_reference_curve(
        const QList<SciQLopPyBuffer>& dimensions,
        const QString& label = QString(),
        const QColor& color = QColor());

    SciQLopGraphInterface* add_model_curve(
        GetDataPyCallable callable,
        const QString& label = QString(),
        const QColor& color = QColor());

    //! Shows the legend on the first pane only, instead of repeating it in every pane.
    void set_shared_legend(bool shared);
    inline bool shared_legend() const noexcept { return m_shared_legend; }

    void set_linked_crosshairs(bool enabled) noexcept;
    bool linked_crosshairs() const noexcept { return m_linked_crosshairs; }

    void set_time_color_enabled(bool enabled) noexcept;
    bool time_color_enabled() const noexcept { return m_time_color_enabled; }
    void set_time_color_gradient(const QColor& start, const QColor& end) noexcept;
    //! Two-stop gradient of the shared scale, and so of every scalar-coloured curve. It
    //! also colours by time, so one choice serves both modes.
    void set_z_gradient_colors(const QColor& start, const QColor& end);
    //! Preset gradient of the shared scale.
    void set_z_gradient(::ColorGradient gradient);
    ::ColorGradient z_gradient() const noexcept;

    /*!
     * \brief z_axis The plot's one colour scale, shared by every pane and graph.
     *        It sits next to the last pane and only shows once a graph is coloured
     *        by a scalar.
     */
    inline virtual SciQLopPlotAxisInterface* z_axis() const noexcept override
    {
        return m_plots.isEmpty() ? nullptr : m_plots.last()->z_axis();
    }
    //! While on (the default) the scale range follows the coloured data of all graphs;
    //! setting the range through z_axis() switches it off.
    bool z_auto_range() const noexcept;
    void set_z_auto_range(bool enabled);
#ifndef BINDINGS_H
    //! For the graphs only: called when their colour scalar changes or they go away.
    void update_color_scale();
#endif

    /*!
     * \brief set_time_marker Highlight, on every subplot, the trajectory point
     *        closest to \a t. A NaN \a t clears the markers.
     */
    Q_SLOT void set_time_marker(double t);
    Q_SLOT void clear_time_marker();

    /*!
     * \brief time_marker_key Time the markers were last positioned at, NaN when cleared.
     * \note This is the requested time, not a promise that a marker is visible:
     *       a subplot whose curve carries no time values has nothing to mark.
     */
    inline double time_marker_key() const noexcept { return m_time_marker_key; }

#ifdef BINDINGS_H
signals:
#endif
    //! Emitted when the marker moves to a new time, with NaN when it is cleared.
    Q_SIGNAL void time_marker_changed(double t);
    Q_SIGNAL void axis_linking_changed(ProjectionAxisLinking linking);
#ifdef BINDINGS_H
public:
#endif

    inline virtual SciQLopPlotAxisInterface* time_axis() const noexcept override
    {
        return m_time_axis;
    }

    //! No x or y axes of its own: each subplot has its pair, reach them through subplot().
    inline SciQLopPlotAxisInterface* x_axis() const noexcept override { return nullptr; }
    inline SciQLopPlotAxisInterface* x2_axis() const noexcept override { return nullptr; }
    inline SciQLopPlotAxisInterface* y_axis() const noexcept override { return nullptr; }
    inline SciQLopPlotAxisInterface* y2_axis() const noexcept override { return nullptr; }

    void set_theme(SciQLopTheme* theme) override;
    inline SciQLopTheme* theme() const override { return m_theme; }

    void export_paint(QPainter* painter, const QRect& target, SciQLopExportTarget kind) override;

    bool save(const QString& filename, int width = 0, int height = 0, double scale = 1.0,
              int quality = -1) override;
    bool save_pdf(const QString& filename, int width = 0, int height = 0) override;
    bool save_png(const QString& filename, int width = 0, int height = 0, double scale = 1.0,
                  int quality = -1) override;
    bool save_jpg(const QString& filename, int width = 0, int height = 0, double scale = 1.0,
                  int quality = -1) override;
    bool save_bmp(const QString& filename, int width = 0, int height = 0,
                  double scale = 1.0) override;
};

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

class SciQLopNDProjectionPlot : public SciQLopPlotInterface
{
    Q_OBJECT

protected:
    SciQLopPlotDummyAxis* m_time_axis = nullptr;
    QList<QColor> m_color_palette;
    int m_color_palette_index = 0;
    QList<SciQLopPlot*> m_plots;
    bool m_linked_axes = false;
    bool m_equal_aspect_ratio = false;
    bool m_linked_crosshairs = false;
    bool m_time_color_enabled = false;
    bool m_enforcing_aspect = false;
    QColor m_time_color_start { 0, 0, 255 };
    QColor m_time_color_end { 255, 0, 0 };
    QList<QCPItemEllipse*> m_time_markers;

    Q_SLOT void _enforce_equal_aspect();
    void _ensure_marker_layer();

    virtual SciQLopGraphInterface*
    plot_impl(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              QObject* sync_with = nullptr,QVariantMap metaData={}) override;

    virtual SciQLopGraphInterface*
    plot_impl(const QList<PyBuffer>& data, QStringList labels = QStringList(),
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

    void set_linked_axes(bool linked) noexcept;

    inline bool linked_axes() const noexcept { return m_linked_axes; }

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

    SciQLopGraphInterface* add_reference_curve(
        const QList<PyBuffer>& dimensions,
        const QString& label = QString(),
        const QColor& color = QColor());

    SciQLopGraphInterface* add_model_curve(
        GetDataPyCallable callable,
        const QString& label = QString(),
        const QColor& color = QColor());

    void set_linked_crosshairs(bool enabled) noexcept;
    bool linked_crosshairs() const noexcept { return m_linked_crosshairs; }

    void set_time_color_enabled(bool enabled) noexcept;
    bool time_color_enabled() const noexcept { return m_time_color_enabled; }
    void set_time_color_gradient(const QColor& start, const QColor& end) noexcept;

    Q_SLOT void set_time_marker(double t);
    Q_SLOT void clear_time_marker();

    inline virtual SciQLopPlotAxisInterface* time_axis() const noexcept override
    {
        return m_time_axis;
    }
};

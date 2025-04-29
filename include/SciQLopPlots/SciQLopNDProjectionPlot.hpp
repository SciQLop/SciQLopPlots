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

    void set_linked_axes(bool linked) noexcept;

    inline bool linked_axes() const noexcept { return m_linked_axes; }

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

    inline virtual SciQLopPlotAxisInterface* time_axis() const noexcept override
    {
        return m_time_axis;
    }
};

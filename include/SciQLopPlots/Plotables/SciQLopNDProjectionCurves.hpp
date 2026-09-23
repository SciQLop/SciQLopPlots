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

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopLineGraph.hpp"
#include "SciQLopPlots/enums.hpp"
#include <qcustomplot.h>

#include "SciQLopPlots/Plotables/SciQLopCurve.hpp"


class SciQLopNDProjectionCurves : public SciQLopGraphInterface
{
    Q_OBJECT
    QList<SciQLopCurve*> m_curves;

    void _update_color_scale();
    void _set_scale_gradient(::ColorGradient gradient);


public:
    explicit SciQLopNDProjectionCurves(SciQLopPlotInterface* parent, QList<SciQLopPlot*>& plots, const QStringList& labels, QVariantMap metaData={});
    virtual ~SciQLopNDProjectionCurves() override;

    Q_SLOT virtual void set_data(const QList<SciQLopPyBuffer>& data) override;
    virtual void set_selected(bool selected) noexcept override;
    virtual  bool selected() const noexcept override;

    virtual void set_colors(const QList<QColor>& colors) override;

    virtual void set_visible(bool visible) noexcept override;
    virtual bool visible() const noexcept override;

    //! One component per pane, in pane order.
    virtual QList<SciQLopGraphComponentInterface*> components() const noexcept override;
    virtual SciQLopGraphComponentInterface* component(int index) const noexcept override;
    virtual SciQLopGraphComponentInterface* component(const QString& name) const noexcept override;
    virtual QList<QColor> colors() const noexcept override;

    /*!
     * \brief set_color_data Tint every pane's curve with \a values through \a gradient.
     * \param values One value per data point (the panes share the same points). An
     *        empty buffer turns the colouring off.
     * \param gradient Applied to the plot's shared scale, so it replaces an earlier
     *        set_z_gradient() choice, the default included. The `3n` data layout has no
     *        such argument and keeps whatever the plot already uses.
     * \throws std::invalid_argument if \a values does not match the data length.
     */
    Q_SLOT virtual void set_color_data(SciQLopPyBuffer values,
                                       ::ColorGradient gradient = ::ColorGradient::Jet) override;
    //! Preset gradient for the scalar colouring, e.g. of the `3n` data layout.
    void set_color_gradient(::ColorGradient gradient);

    void set_line_width(qreal width);
    qreal line_width() const;
#ifndef BINDINGS_H
    //! Called by the plot: every pane's curve follows the plot's one shared scale.
    void attach_color_scale(QCPColorScale* scale) override;
    bool has_color_values() const override;
    std::optional<std::pair<double, double>> color_range(bool log) const override;
#endif

    void set_time_color_enabled(bool enabled);
    bool time_color_enabled() const;
    void set_time_color_gradient(const QColor& start, const QColor& end);
    //! One entry per subplot: the QPointF to mark, or an invalid QVariant.
    QList<QVariant> positions_at_time(double t) const;
};

class SciQLopNDProjectionCurvesFunction :public SciQLopNDProjectionCurves, public SciQLopFunctionGraph
{
    Q_OBJECT

    inline Q_SLOT void _set_data(QList<SciQLopPyBuffer> data)
    {
        SciQLopNDProjectionCurves::set_data(data);
    }

public:
    explicit SciQLopNDProjectionCurvesFunction(SciQLopPlotInterface* parent, QList<SciQLopPlot*>& plots,
                                  GetDataPyCallable&& callable, const QStringList& labels, QVariantMap metaData={});

    virtual ~SciQLopNDProjectionCurvesFunction() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

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

#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-multigraph.h>
#include <QPointer>
#include <QSignalBlocker>
#include <memory>
#include <optional>
#include <vector>

class SciQLopLineGraph : public SciQLopMultiGraphBase
{
    Q_OBJECT

    std::shared_ptr<const std::vector<double>> _color_values;
    //! What _color_values was made from, handed back as is by color_data().
    SciQLopPyBuffer _color_buffer;
    ::ColorGradient _gradient_preset = ::ColorGradient::Jet;
    QCPColorGradient _color_gradient { QCPColorGradient::gpJet };
    QPointer<QCPColorScale> _color_scale;

    //! Range, scale type and gradient into the multigraph: the scale's, or the graph's own.
    void push_color_mapping();
    void check_color_length(const SciQLopPyBuffer& values, std::size_t samples) const;
    //! One value per x sample, or an empty/invalid buffer to turn the colouring off.
    void apply_color_values(const SciQLopPyBuffer& values);

protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) override
    {
        return new QCPMultiGraph(keyAxis, valueAxis);
    }

public:
    explicit SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                              SciQLopPlotAxis* value_axis,
                              const QStringList& labels = QStringList(),
                              QVariantMap metaData = {});
    ~SciQLopLineGraph() override;

    //! A refresh of another length drops the colour values (a same-length one keeps them).
    Q_SLOT void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y) override;
    void set_visible(bool visible) noexcept override;

    /*!
     * \brief set_color_data Colour the line (every component) point by point.
     * \param values One value per x sample, any numeric dtype. NaN leaves a gap. An empty
     *        buffer turns the colouring back off.
     * \throws std::invalid_argument if \a values does not match the number of x samples.
     *
     * On a plot the colour scale is the plot's, shared with its curves, so \a gradient also
     * replaces an earlier plot.set_z_gradient(). A colormap on the plot keeps its own.
     */
    Q_SLOT void set_color_data(SciQLopPyBuffer values,
                               ::ColorGradient gradient = ::ColorGradient::Jet) override;

    /*!
     * \brief set_color_gradient The gradient colour data is drawn with, set_color_data's
     *        or a coloured data batch's. It can be set before any data arrives.
     */
    void set_color_gradient(::ColorGradient gradient);

    //! The colour values last set (set_color_data or a coloured batch), None when uncoloured.
    SciQLopPyBuffer color_data() const noexcept { return _color_values ? _color_buffer : SciQLopPyBuffer {}; }
    //! The gradient given to this graph, Jet when none was. On a plot, plot.z_gradient() is
    //! the one drawn.
    ::ColorGradient color_gradient() const noexcept { return _gradient_preset; }

    /*!
     * \brief set_data_and_color A batch that carries its colour axis: \a data is [x, y],
     *        \a color one value per x sample, drawn with the stored gradient.
     * \throws std::invalid_argument before touching the graph if the batch is malformed.
     */
    void set_data_and_color(const QList<SciQLopPyBuffer>& data,
                            const SciQLopPyBuffer& color) override;

#ifndef BINDINGS_H
    bool has_color_values() const override { return _color_values != nullptr; }
    std::optional<std::pair<double, double>> color_range(bool log) const override;
    void attach_color_scale(QCPColorScale* scale) override;
#endif
};

class SciQLopLineGraphFunction : public SciQLopLineGraph,
                                 public SciQLopFunctionGraph
{
    Q_OBJECT
public:
    explicit SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                      SciQLopPlotAxis* value_axis, GetDataPyCallable&& callable,
                                      const QStringList& labels, QVariantMap metaData = {});
    ~SciQLopLineGraphFunction() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

class SciQLopLineGraphRemote : public SciQLopLineGraph,
                               public SciQLopRemoteGraph
{
    Q_OBJECT
public:
    explicit SciQLopLineGraphRemote(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                    SciQLopPlotAxis* value_axis,
                                    const QStringList& labels = QStringList(),
                                    QVariantMap metaData = {});
    ~SciQLopLineGraphRemote() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }

    // busy() reports the remote request/response lifecycle via the mixin flag
    // so it is valid even before QCP components exist (first set_data).
    // set_busy also forwards to the base so the component's own busy flag
    // stays consistent once components exist -- but the base's setBusy()
    // already drives busy_changed via the QCPMultiGraph's busyChanged signal
    // whenever it isn't a no-op, so calling it unconditionally would
    // double-emit. Block that forwarding and always emit exactly once below,
    // driven by the authoritative remote_busy() state.
    inline bool busy() const noexcept override { return remote_busy(); }
    inline void set_busy(bool busy) noexcept override
    {
        set_remote_busy(busy);
        {
            const QSignalBlocker blocker(this);
            SciQLopLineGraph::set_busy(busy);
        }
        Q_EMIT busy_changed(busy);
    }
};

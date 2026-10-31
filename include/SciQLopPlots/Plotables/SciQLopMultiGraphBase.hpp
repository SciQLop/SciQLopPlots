/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-multigraph.h>
#include <map>
#include <optional>
#include <span>

class SciQLopMultiGraphBase : public SQPQCPAbstractPlottableWrapper
{
    Q_OBJECT
protected:
    QCPMultiGraph* _multiGraph = nullptr;
    SciQLopPyBuffer _x, _y;
    QStringList _pendingLabels;
    SciQLopPlotAxis* _keyAxis = nullptr;
    SciQLopPlotAxis* _valueAxis = nullptr;

    struct ColumnPreset
    {
        std::optional<GraphLineStyle> line_style;
        std::optional<GraphMarkerShape> marker;
        std::optional<bool> in_legend;
        std::optional<qreal> line_width;
        std::optional<qreal> marker_size;
    };
    std::map<int, ColumnPreset> _column_presets;
    //! Graph-wide styles, applied before each column's own preset.
    ColumnPreset _graph_preset;

    void apply_style(int column, const ColumnPreset& preset);
    void apply_preset(int column);
    void apply_presets_to_all();
    void apply_presets_to_new_components(int first, int last);

    virtual QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) = 0;

    void clear_graphs(bool graph_already_removed = false);
    void build_data_source(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y);
    void sync_components();

public:
    explicit SciQLopMultiGraphBase(const QString& type_label, QCustomPlot* parent,
                                   SciQLopPlotAxis* key_axis, SciQLopPlotAxis* value_axis,
                                   const QStringList& labels, QVariantMap metadata);
    ~SciQLopMultiGraphBase() override;

    Q_SLOT void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y) override;
    QList<SciQLopPyBuffer> data() const noexcept override;

    std::size_t line_count() const noexcept
    {
        return _multiGraph ? _multiGraph->componentCount() : 0;
    }

protected:
    // Propagate the user-facing name to the underlying QCPMultiGraph so the
    // legend shows it even before any data/components arrive (the loading cue),
    // instead of falling back to a meaningless placeholder.
    void _named(const QString& name) override
    {
        SQPQCPAbstractPlottableWrapper::_named(name);
        if (_multiGraph)
            _multiGraph->setName(name);
    }

public:
    // Busy belongs to the QCPMultiGraph, not to the components: components are
    // only created on the first set_data, and each one wraps this very same
    // plottable anyway. The base implementation walks m_components, so it is a
    // silent no-op for the whole first fetch — exactly the window the activity
    // indicator is for (right after a product drop). Mirrors
    // SciQLopColorMapBase, which routes through its single plottable().
    bool busy() const noexcept override { return _multiGraph && _multiGraph->busy(); }

    void set_busy(bool busy) noexcept override
    {
        if (_multiGraph)
            _multiGraph->setBusy(busy);
    }

    //! A key step larger than this many times the smaller neighbouring step breaks
    //! the lines (default 1.5). 0 turns gap detection off, e.g. for step or state data.
    void set_gap_threshold(double threshold);
    double gap_threshold() const noexcept;

    void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

    void create_graphs(const QStringList& labels);

    void set_fill_to_value(int column, double value);
    void set_fill_between(int column, int other);
    void clear_fill(int column);
    void set_fill_color(int column, const QColor& color);
    void set_error_bars(int column, int lower, int upper, double cap_width = 6.);
    void clear_error_bars(int column);

    //! Styles for components that may not exist yet (a callback graph before its first data):
    //! applied now if the component exists, and again whenever it is created.
    void set_column_line_style(int column, GraphLineStyle style);
    void set_column_marker_shape(int column, GraphMarkerShape shape);
    void set_column_in_legend(int column, bool in_legend);
    //! The preset if one was set, else the component's flag, else true.
    bool column_in_legend(int column) const;

    //! Graph-wide styles: every component now and every one created later. A column preset or
    //! a component setter called afterwards still wins for its component.
    void set_line_width(qreal width);
    qreal line_width() const;
    void set_marker_shape(GraphMarkerShape shape);
    GraphMarkerShape marker_shape() const;
    void set_marker_size(qreal size);
    qreal marker_size() const;

#ifndef BINDINGS_H
    void collect_visible_values(const SciQLopPlotRange& visible_key_range,
                                std::vector<double>& out) const noexcept override;
#endif
};

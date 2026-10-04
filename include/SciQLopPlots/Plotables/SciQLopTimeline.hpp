/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#pragma once
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include <QColor>
#include <QMap>
#include <QPointer>
#include <qcustomplot.h>
#include <plottables/plottable-intervals.h>

class SciQLopPlotAxis;
class QMouseEvent;

class SciQLopTimeline : public SciQLopPlottableInterface
{
    Q_OBJECT
    QPointer<QCPIntervals> _intervals;
    QPointer<QCPLaneLayout> _layout;
    SciQLopPlotAxis* _y_axis;
    qint64 _hovered = -1;
    QStringList _category_order;

    void _apply_category_order();

    void _apply_palette();
#ifndef BINDINGS_H
    void _update_hover(QMouseEvent* event);
    void _set_hovered(qint64 id);
    bool eventFilter(QObject* watched, QEvent* event) override;
#endif

public:
    SciQLopTimeline(QCustomPlot* plot, QCPLaneLayout* layout, SciQLopPlotAxis* yAxis,
                    QVariantMap metaData = {});
    ~SciQLopTimeline() override;

    void set_intervals_coded(SciQLopPyBuffer start, SciQLopPyBuffer stop,
                             SciQLopPyBuffer lane_codes, const QStringList& lane_names,
                             SciQLopPyBuffer category_codes, const QStringList& category_names,
                             const QStringList& labels, SciQLopPyBuffer ids);
    int count() const;

    QStringList lanes() const;
    void set_lanes(const QStringList& lanes);
    bool rename_lane(const QString& from, const QString& to);
    int lane_height() const;
    void set_lane_height(int px);

    void set_category_colors(const QMap<QString, QColor>& colors);
    QColor category_color(const QString& category) const;

    QString layer() const noexcept override;
    void set_visible(bool visible) noexcept override;
    bool visible() const noexcept override;
    SciQLopPlotAxisInterface* y_axis() const noexcept override;
    bool selected() const noexcept override;
    void set_selected(bool selected) noexcept override;

    QList<qint64> selected_ids() const;
    void select_ids(const QList<qint64>& ids);
    QPointF pixel_of(double key, const QString& lane) const;
    //! id, start, stop, duration, lane, category and label of interval \a id; empty if unknown.
    QVariantMap interval_info(qint64 id) const;

    bool editable() const;
    void set_editable(bool editable);
    QStringList edit_modes() const;
    void set_edit_modes(const QStringList& names);
    void set_snap_edges();
    void set_snap_step(double seconds);
    void clear_snap();
    //! Snap dragged edges to these times only (epoch seconds), e.g. orbit events.
    void set_snap_times(const QList<double>& times);
    QList<double> snap_times() const;
    QString snap_mode() const;
    double snap_step() const;
    //! "wave" (logic-analyzer look, the default) or "bars"; other names are ignored.
    QString style() const;
    void set_style(const QString& name);
    //! Sub-rows for overlaps: "" (none), "time" (first free sub-row) or "category" (one fixed
    //! sub-row per category); other names are ignored.
    QString stack() const;
    void set_stack(const QString& mode);
    //! Category names stacked first, in this order; the others follow, first seen first.
    QStringList category_order() const { return _category_order; }
    void set_category_order(const QStringList& names);
    //! Edits stop at the neighbouring block in the same row instead of overlapping it.
    bool forbid_overlap() const;
    void set_forbid_overlap(bool forbid);

#ifndef BINDINGS_H
    QCPIntervals* intervals() const { return _intervals; }
    qint64 id_of_row(int row) const { return _intervals->columns().ids[row]; }
#endif

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void lanes_changed();
    Q_SIGNAL void hovered(qint64 id);
    Q_SIGNAL void selected_intervals_changed(QList<qint64> ids);
    Q_SIGNAL void intervals_changed(const QVariantList& edits);
    Q_SIGNAL void interval_created(double start, double stop, const QString& lane);
    //! Same, with the category of the row it was drawn in (stack="category"), else "".
    Q_SIGNAL void interval_created_in(double start, double stop, const QString& lane,
                                      const QString& category);
    Q_SIGNAL void delete_requested(const QList<qint64>& ids);
};

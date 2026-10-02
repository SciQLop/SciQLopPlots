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

class SciQLopTimeline : public SciQLopPlottableInterface
{
    Q_OBJECT
    QPointer<QCPIntervals> _intervals;
    QPointer<QCPLaneLayout> _layout;
    SciQLopPlotAxis* _y_axis;

    void _apply_palette();

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

#ifndef BINDINGS_H
    QCPIntervals* intervals() const { return _intervals; }
    qint64 id_of_row(int row) const { return _intervals->columns().ids[row]; }
#endif

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void lanes_changed();
};

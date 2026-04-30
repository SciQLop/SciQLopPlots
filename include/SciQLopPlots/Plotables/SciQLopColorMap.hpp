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
#include "SciQLopPlots/DataProducer/DataProducer.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include "SciQLopColorMapBase.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/enums.hpp"
#include <plottables/plottable-colormap2.h>
#include <datasource/soa-datasource-2d.h>
#include <memory>
#include <span>

class SciQLopColorMap : public SciQLopColorMapBase
{
    QPointer<QCPColorMap2> _cmap;
    bool _auto_scale_y = false;

    struct DataSourceWithBuffers {
        PyBuffer x, y, z;
        std::shared_ptr<QCPAbstractDataSource2D> source;
    };
    std::shared_ptr<DataSourceWithBuffers> _dataHolder;

    Q_OBJECT

    void _cmap_got_destroyed();

protected:
    virtual QCPAbstractPlottable* plottable() const override
    {
        return _cmap.data();
    }

public:
    explicit SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* xAxis, SciQLopPlotAxis* yAxis,
                             SciQLopPlotColorScaleAxis* zAxis, const QString& name,
                             QVariantMap metaData = {});
    virtual ~SciQLopColorMap() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline QCPColorMap2* colorMap() const { return _cmap; }

    void set_auto_scale_y(bool auto_scale_y);
    inline bool auto_scale_y() const { return _auto_scale_y; }

    inline void set_contour_levels(const QVector<double>& levels)
    {
        if (_cmap) _cmap->setContourLevels(levels);
    }

    inline QVector<double> contour_levels() const
    {
        if (_cmap) return _cmap->contourLevels();
        return {};
    }

    inline void set_auto_contour_levels(int count)
    {
        if (_cmap)
        {
            _cmap->setAutoContourLevels(count);
            Q_EMIT contour_levels_changed();
        }
    }

    inline int auto_contour_level_count() const
    {
        return _cmap ? _cmap->autoContourLevelCount() : 0;
    }

    inline void set_contour_pen(const QPen& pen)
    {
        if (_cmap)
        {
            _cmap->setContourPen(pen);
            Q_EMIT contour_pen_changed(pen);
        }
    }

    inline QPen contour_pen() const
    {
        return _cmap ? _cmap->contourPen() : QPen();
    }

    inline void set_contour_color(const QColor& color)
    {
        if (_cmap)
        {
            auto pen = _cmap->contourPen();
            pen.setColor(color);
            _cmap->setContourPen(pen);
            Q_EMIT contour_pen_changed(pen);
        }
    }

    inline QColor contour_color() const
    {
        return _cmap ? _cmap->contourPen().color() : QColor();
    }

    inline void set_contour_width(double width)
    {
        if (_cmap)
        {
            auto pen = _cmap->contourPen();
            pen.setWidthF(width);
            _cmap->setContourPen(pen);
            Q_EMIT contour_pen_changed(pen);
        }
    }

    inline double contour_width() const
    {
        return _cmap ? _cmap->contourPen().widthF() : 1.0;
    }

    inline void set_contour_labels_enabled(bool enabled)
    {
        if (_cmap)
        {
            _cmap->setContourLabelEnabled(enabled);
            Q_EMIT contour_labels_enabled_changed(enabled);
        }
    }

    inline bool contour_labels_enabled() const
    {
        return _cmap ? _cmap->contourLabelEnabled() : false;
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void auto_scale_y_changed(bool);
    Q_SIGNAL void contour_levels_changed();
    Q_SIGNAL void contour_pen_changed(const QPen& pen);
    Q_SIGNAL void contour_labels_enabled_changed(bool enabled);

private:
    ::DataOrder _dataOrder = DataOrder::RowMajor;
};


class SciQLopColorMapFunction : public SciQLopColorMap, public SciQLopFunctionGraph
{
    Q_OBJECT

    inline Q_SLOT void _set_data(PyBuffer x, PyBuffer y, PyBuffer z)
    {
        SciQLopColorMap::set_data(x, y, z);
    }

public:
    explicit SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                     SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                     GetDataPyCallable&& callable, const QString& name);

    virtual ~SciQLopColorMapFunction() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

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
#include "SciQLopPlots/DataProducer/DataProducer.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include "SciQLopPlots/enums.hpp"
#include "SciQLopColorMapBase.hpp"
#include <qcustomplot.h>
#include <plottables/plottable-histogram2d.h>
#include <datasource/soa-datasource.h>
#include <memory>
#include <span>

class SciQLopHistogram2D : public SciQLopColorMapBase
{
    QPointer<QCPHistogram2D> _hist;

    struct DataSourceWithBuffers
    {
        SciQLopPyBuffer x, y;
        std::shared_ptr<QCPAbstractDataSource> source;
    };
    std::shared_ptr<DataSourceWithBuffers> _dataHolder;

    Q_OBJECT

    void _hist_got_destroyed();

protected:
    virtual QCPAbstractPlottable* plottable() const override
    {
        return _hist.data();
    }

public:
    explicit SciQLopHistogram2D(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                const QString& name, int x_bins = 100, int y_bins = 100,
                                QVariantMap metaData = {});
    virtual ~SciQLopHistogram2D() override;

    Q_SLOT virtual void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y) override;
    virtual QList<SciQLopPyBuffer> data() const noexcept override;

    inline QCPHistogram2D* histogram() const { return _hist; }

    void set_bins(int x_bins, int y_bins);
    int x_bins() const;
    int y_bins() const;

    void set_normalization(int normalization);
    int normalization() const;

    // Logarithmic bin spacing per axis. Bins are spaced evenly in log10 and
    // non-positive samples are dropped; meant to accompany a log-scaled axis.
    void set_x_bins_log(bool log);
    void set_y_bins_log(bool log);
    bool x_bins_log() const;
    bool y_bins_log() const;

    SciQLopPlotRange z_percentile_range(const SciQLopPlotRange& x_range,
                                        const SciQLopPlotRange& y_range, double low,
                                        double high) const noexcept override;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void bins_changed(int x_bins, int y_bins);
    Q_SIGNAL void normalization_changed(int normalization);
    Q_SIGNAL void x_bins_log_changed(bool log);
    Q_SIGNAL void y_bins_log_changed(bool log);
};


class SciQLopHistogram2DFunction : public SciQLopHistogram2D, public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopHistogram2DFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                        SciQLopPlotAxis* yAxis,
                                        SciQLopPlotColorScaleAxis* zAxis,
                                        GetDataPyCallable&& callable, const QString& name,
                                        int x_bins = 100, int y_bins = 100);

    virtual ~SciQLopHistogram2DFunction() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

class SciQLopHistogram2DRemote : public SciQLopHistogram2D, public SciQLopRemoteGraph
{
    Q_OBJECT
public:
    explicit SciQLopHistogram2DRemote(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                      SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                      const QString& name, int x_bins = 100, int y_bins = 100,
                                      QVariantMap metaData = {});
    ~SciQLopHistogram2DRemote() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }

    inline bool busy() const noexcept override { return remote_busy(); }
    inline void set_busy(bool busy) noexcept override
    {
        set_remote_busy(busy);
        SciQLopHistogram2D::set_busy(busy);
        Q_EMIT busy_changed(busy);
    }
};

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
#include "SciQLopPlots/ThreadGuard.hpp"
#include <plottables/plottable-colormap2.h>
#include <datasource/soa-datasource-2d.h>
#include <QSignalBlocker>
#include <memory>
#include <span>

class SciQLopColorMap : public SciQLopColorMapBase
{
    QPointer<QCPColorMap2> _cmap;

    struct DataSourceWithBuffers {
        SciQLopPyBuffer x, y, z;
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

    Q_SLOT virtual void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z) override;
    virtual QList<SciQLopPyBuffer> data() const noexcept override;

    inline QCPColorMap2* colorMap() const { return _cmap; }

    SciQLopPlotRange z_percentile_range(const SciQLopPlotRange& x_range,
                                        const SciQLopPlotRange& y_range, double low,
                                        double high) const noexcept override;
};


class SciQLopColorMapFunction : public SciQLopColorMap, public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                     SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                     GetDataPyCallable&& callable, const QString& name);

    virtual ~SciQLopColorMapFunction() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

class SciQLopColorMapRemote : public SciQLopColorMap, public SciQLopRemoteGraph
{
    Q_OBJECT
public:
    explicit SciQLopColorMapRemote(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                   SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                   const QString& name, QVariantMap metaData = {});
    ~SciQLopColorMapRemote() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }

    inline bool busy() const noexcept override { return remote_busy(); }
    // SciQLopColorMap::set_busy() forwards to the underlying QCPColorMap2,
    // whose busyChanged is already connected to busy_changed in
    // SciQLopColorMap's ctor -- so calling it here would double-emit. Block
    // that forwarding and always emit exactly once below, driven by the
    // authoritative remote_busy() state (this also covers the window before
    // the QCPColorMap2 exists, where the base call is a no-op anyway).
    inline void set_busy(bool busy) noexcept override
    {
        set_remote_busy(busy);
        {
            const QSignalBlocker blocker(this);
            SciQLopColorMap::set_busy(busy);
        }
        Q_EMIT busy_changed(busy);
    }
};

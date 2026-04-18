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

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void auto_scale_y_changed(bool);

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
};

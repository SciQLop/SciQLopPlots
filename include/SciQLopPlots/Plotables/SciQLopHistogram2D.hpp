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
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include "SciQLopPlots/enums.hpp"
#include "SciQLopGraphInterface.hpp"
#include <qcustomplot.h>
#include <plottables/plottable-histogram2d.h>
#include <datasource/soa-datasource.h>
#include <memory>
#include <span>

class SciQLopHistogram2D : public SciQLopColorMapInterface
{
    bool _got_first_data = false;
    bool _selected = false;

    QTimer* _icon_update_timer;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    SciQLopPlotColorScaleAxis* _colorScaleAxis;
    QPointer<QCPHistogram2D> _hist;

    struct DataSourceWithBuffers
    {
        PyBuffer x, y;
        std::shared_ptr<QCPAbstractDataSource> source;
    };
    std::shared_ptr<DataSourceWithBuffers> _dataHolder;

    Q_OBJECT
    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    void _hist_got_destroyed();

    inline QCPPlottableLegendItem* _legend_item()
    {
        if (_hist)
        {
            auto plot = _plot();
            return plot->legend->itemWithPlottable(_hist.data());
        }
        return nullptr;
    }

public:
    explicit SciQLopHistogram2D(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                const QString& name, int key_bins = 100, int value_bins = 100,
                                QVariantMap metaData = {});
    virtual ~SciQLopHistogram2D() override;

    inline virtual QString layer() const noexcept override
    {
        if (_hist)
            return _hist->layer()->name();
        return QString();
    }

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline QCPHistogram2D* histogram() const { return _hist; }

    void set_bins(int key_bins, int value_bins);
    int key_bins() const;
    int value_bins() const;

    void set_normalization(int normalization);
    int normalization() const;

    inline virtual ColorGradient gradient() const noexcept override
    {
        return _colorScaleAxis->color_gradient();
    }

    inline virtual void set_gradient(ColorGradient gradient) noexcept override
    {
        _colorScaleAxis->set_color_gradient(gradient);
    }

    virtual void set_selected(bool selected) noexcept override;
    virtual bool selected() const noexcept override;

    virtual bool busy() const noexcept override
    {
        return _hist ? _hist->busy() : false;
    }

    virtual void set_busy(bool busy) noexcept override
    {
        if (_hist)
            _hist->setBusy(busy);
    }

    inline virtual void set_name(const QString& name) noexcept override
    {
        this->setObjectName(name);
        if (_hist)
            _hist->setName(name);
    }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }
    virtual SciQLopPlotAxisInterface* z_axis() const noexcept override { return _colorScaleAxis; }
};

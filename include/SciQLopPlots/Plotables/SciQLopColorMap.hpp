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
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/enums.hpp"
#include <QMutex>
#include <qcustomplot.h>

struct ColormapResampler;
class QThread;

class SciQLopColorMap : public SciQLopColorMapInterface
{
    bool _selected = false;
    ColormapResampler* _resampler = nullptr;
    QThread* _resampler_thread = nullptr;

    QTimer* _icon_update_timer;

    QCPRange _data_x_range;
    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    SciQLopPlotColorScaleAxis* _colorScaleAxis;
    QPointer<QCPColorMap> _cmap;
    QMutex _data_swap_mutex;
    bool _auto_scale_y = false;

    ColorGradient _gradient;

    Q_OBJECT
    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    void _resample(const QCPRange& newRange);
    void _cmap_got_destroyed();

    inline QCPPlottableLegendItem* _legend_item()
    {
        if (_cmap)
        {
            auto plot = _plot();
            return plot->legend->itemWithPlottable(_cmap.data());
        }
        return nullptr;
    }

    Q_SLOT void _setGraphData(QCPColorMapData* data);

public:
    Q_ENUMS(FractionStyle)
    explicit SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                             SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis, const QString& name);
    virtual ~SciQLopColorMap() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z) override;

    inline QCPColorMap* colorMap() const { return _cmap; }

    void set_auto_scale_y(bool auto_scale_y);

    inline bool auto_scale_y() const { return _auto_scale_y; }

    inline virtual ColorGradient gradient() const noexcept override { return _gradient; }

    inline virtual void set_gradient(ColorGradient gradient) noexcept override
    {
        _gradient = gradient;
        if (_cmap)
        {
            QCPColorGradient new_gradient;
            switch (gradient)
            {

                case ColorGradient::Grayscale:
                    new_gradient = QCPColorGradient::gpGrayscale;
                    break;
                case ColorGradient::Hot:
                    new_gradient = QCPColorGradient::gpHot;
                    break;
                case ColorGradient::Cold:
                    new_gradient = QCPColorGradient::gpCold;
                    break;
                case ColorGradient::Night:
                    new_gradient = QCPColorGradient::gpNight;
                    break;
                case ColorGradient::Candy:
                    new_gradient = QCPColorGradient::gpCandy;
                    break;
                case ColorGradient::Geography:
                    new_gradient = QCPColorGradient::gpGeography;
                    break;
                case ColorGradient::Ion:
                    new_gradient = QCPColorGradient::gpIon;
                    break;
                case ColorGradient::Thermal:
                    new_gradient = QCPColorGradient::gpThermal;
                    break;
                case ColorGradient::Polar:
                    new_gradient = QCPColorGradient::gpPolar;
                    break;
                case ColorGradient::Spectrum:
                    new_gradient = QCPColorGradient::gpSpectrum;
                    break;
                case ColorGradient::Jet:
                    new_gradient = QCPColorGradient::gpJet;
                    break;
                case ColorGradient::Hues:
                    new_gradient = QCPColorGradient::gpHues;
                    break;
            }
            new_gradient.setNanHandling(QCPColorGradient::nhTransparent);
            _cmap->setGradient(new_gradient);
            _cmap->rescaleDataRange(true);
        }
    }

    virtual void set_selected(bool selected) noexcept override;
    virtual bool selected() const noexcept override;

    inline virtual void set_name(const QString& name) noexcept override
    {
        this->setObjectName(name);
        if (_cmap)
            _cmap->setName(name);
    }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }

    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

    virtual SciQLopPlotAxisInterface* z_axis() const noexcept override { return _colorScaleAxis; }


#ifndef BINDINGS_H
    Q_SIGNAL void auto_scale_y_changed(bool);
#endif

private:
    ::DataOrder _dataOrder = DataOrder::RowMajor;
};

class SciQLopColorMapFunction : public SciQLopColorMap
{
    Q_OBJECT
    SimplePyCallablePipeline* m_pipeline;

    inline Q_SLOT void _set_data(PyBuffer x, PyBuffer y, PyBuffer z)
    {
        SciQLopColorMap::set_data(x, y, z);
    }

public:
    explicit SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                     SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis, GetDataPyCallable&& callable,
                                     const QString& name);

    virtual ~SciQLopColorMapFunction() override = default;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z) override;
    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) override;
};

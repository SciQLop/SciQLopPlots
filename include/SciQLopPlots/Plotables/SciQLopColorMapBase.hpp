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
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <qcustomplot.h>

class SciQLopColorMapBase : public SciQLopColorMapInterface
{
    Q_OBJECT

protected:
    bool _selected = false;
    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    SciQLopPlotColorScaleAxis* _colorScaleAxis;

    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    inline QCPPlottableLegendItem* _legend_item()
    {
        if (auto* p = plottable(); p)
        {
            auto plot = _plot();
            return plot->legend->itemWithPlottable(p);
        }
        return nullptr;
    }

    virtual QCPAbstractPlottable* plottable() const = 0;

public:
    SciQLopColorMapBase(SciQLopPlotAxis* keyAxis, SciQLopPlotAxis* valueAxis,
                        SciQLopPlotColorScaleAxis* colorScaleAxis,
                        QVariantMap metaData, QObject* parent);
    virtual ~SciQLopColorMapBase() override;

    inline virtual QString layer() const noexcept override
    {
        if (auto* p = plottable(); p)
            return p->layer()->name();
        return QString();
    }

    inline virtual void set_name(const QString& name) noexcept override
    {
        this->setObjectName(name);
        if (auto* p = plottable(); p)
            p->setName(name);
    }

    virtual void set_selected(bool selected) noexcept override;
    virtual bool selected() const noexcept override;

    virtual bool busy() const noexcept override
    {
        if (auto* p = plottable(); p)
            return p->busy();
        return false;
    }

    virtual void set_busy(bool busy) noexcept override
    {
        if (auto* p = plottable(); p)
            p->setBusy(busy);
    }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }
    virtual SciQLopPlotAxisInterface* z_axis() const noexcept override { return _colorScaleAxis; }

    inline virtual ColorGradient gradient() const noexcept override
    {
        return _colorScaleAxis->color_gradient();
    }

    inline virtual void set_gradient(ColorGradient gradient) noexcept override
    {
        _colorScaleAxis->set_color_gradient(gradient);
    }

    inline virtual void set_visible(bool visible) noexcept override
    {
        if (auto* p = plottable(); p && p->visible() != visible)
        {
            p->setVisible(visible);
            Q_EMIT visible_changed(visible);
            if (auto* plot = _plot())
                plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }

    inline virtual bool visible() const noexcept override
    {
        if (auto* p = plottable(); p)
            return p->visible();
        return false;
    }

    inline virtual void set_y_log_scale(bool y_log_scale) noexcept override
    {
        if (_valueAxis)
            _valueAxis->set_log(y_log_scale);
    }

    inline virtual bool y_log_scale() const noexcept override
    {
        return _valueAxis ? _valueAxis->log() : false;
    }

    inline virtual void set_z_log_scale(bool z_log_scale) noexcept override
    {
        if (_colorScaleAxis)
            _colorScaleAxis->set_log(z_log_scale);
    }

    inline virtual bool z_log_scale() const noexcept override
    {
        return _colorScaleAxis ? _colorScaleAxis->log() : false;
    }
};

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
#include <QPointer>
#include <optional>
#include <qcustomplot.h>

class QMouseEvent;

class SciQLopColorMapBase : public SciQLopColorMapInterface
{
    Q_OBJECT

protected:
    bool _selected = false;
    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    // QPointer because the color-scale axis is owned by the parent QCustomPlot
    // and may be destroyed before us in atypical teardown orders.
    QPointer<SciQLopPlotColorScaleAxis> _colorScaleAxis;
    double _autoscale_percentile_low = 0.;
    double _autoscale_percentile_high = 100.;

    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    inline QCPPlottableLegendItem* _legend_item()
    {
        if (auto* p = plottable(); p)
        {
            auto plot = _plot();
            if (!plot)
                return nullptr;
            return plot->legend->itemWithPlottable(p);
        }
        return nullptr;
    }

    virtual QCPAbstractPlottable* plottable() const = 0;

    void _connect_legend_visibility();
    void _apply_legend_visibility_style(bool visible);

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
        SciQLopPlottableInterface::set_name(name);
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
        return _colorScaleAxis ? _colorScaleAxis->color_gradient() : ColorGradient::Jet;
    }

    inline virtual void set_gradient(ColorGradient gradient) noexcept override
    {
        if (_colorScaleAxis)
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

    void set_autoscale_percentile_low(double percentile) noexcept;
    inline double autoscale_percentile_low() const noexcept { return _autoscale_percentile_low; }
    void set_autoscale_percentile_high(double percentile) noexcept;
    inline double autoscale_percentile_high() const noexcept { return _autoscale_percentile_high; }

    // Range of z over cells whose x/y fall in the given visible ranges, clamped
    // to [low, high] percentiles (nearest-rank). low=0/high=100 gives plain
    // min/max of the visible data. Non-finite cells are skipped. Default returns
    // an empty (NaN) range; concrete plotables override it.
    virtual SciQLopPlotRange z_percentile_range(const SciQLopPlotRange& x_range,
                                                const SciQLopPlotRange& y_range, double low,
                                                double high) const noexcept
    {
        Q_UNUSED(x_range);
        Q_UNUSED(y_range);
        Q_UNUSED(low);
        Q_UNUSED(high);
        return SciQLopPlotRange();
    }

protected:
#ifndef BINDINGS_H
    // nullopt ⇒ caller should use the plain min/max fast path (default 0/100).
    std::optional<SciQLopPlotRange> z_rescale_range() const noexcept;
    // Installs z_rescale_range as the color-scale axis rescale provider.
    void install_rescale_provider() noexcept;
#endif

private Q_SLOTS:
    void _on_legend_double_clicked(QCPLegend* legend, QCPAbstractLegendItem* item,
                                   QMouseEvent* event);
};

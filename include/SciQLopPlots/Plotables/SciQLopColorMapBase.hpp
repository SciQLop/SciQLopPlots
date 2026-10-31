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
    bool m_selected = false;
    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    // QPointer because the color-scale axis is owned by the parent QCustomPlot
    // and may be destroyed before us in atypical teardown orders.
    QPointer<SciQLopPlotColorScaleAxis> _colorScaleAxis;
    double _autoscale_percentile_low = 0.;
    double _autoscale_percentile_high = 100.;
    QMetaObject::Connection _y_log_forward;
    bool _auto_scale_y = false;

    //! With auto_scale_y, fits the y axis to \a range (the data's y extent) when found.
    void _auto_scale_y_to(const QCPRange& range, bool found);

    //! The gradient and log flags live on the axes: their signals become this map's.
    void _forward_axis_signals();

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
    friend QList<QCPAbstractPlottable*> qcp_plottables_of(const SciQLopPlottableInterface*);

    void _connect_legend_visibility();
    void _apply_legend_visibility_style(bool visible);

public:
    SciQLopColorMapBase(SciQLopPlotAxis* keyAxis, SciQLopPlotAxis* valueAxis,
                        SciQLopPlotColorScaleAxis* colorScaleAxis,
                        QVariantMap metadata, QObject* parent);
    virtual ~SciQLopColorMapBase() override;

    inline virtual QString layer() const noexcept override
    {
        if (auto* p = plottable(); p)
            return p->layer()->name();
        return QString();
    }

protected:
    void _named(const QString& name) override
    {
        SciQLopColorMapInterface::_named(name);
        if (auto* p = plottable(); p)
            p->setName(name);
    }

    bool _selected() const override { return m_selected; }
    void _set_selected(bool selected) override;

    bool _visible() const override
    {
        auto* p = plottable();
        return p && p->visible();
    }

    void _set_visible(bool visible) override
    {
        if (auto* p = plottable(); p)
        {
            p->setVisible(visible);
            if (auto* plot = _plot())
                plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }

public:

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

    //! Fit the y axis to the data's y extent on every set_data.
    void set_auto_scale_y(bool auto_scale_y);
    inline bool auto_scale_y() const { return _auto_scale_y; }

    void set_contour_levels(const QVector<double>& levels);
    QVector<double> contour_levels() const;
    void set_auto_contour_levels(int count);
    int auto_contour_level_count() const;
    void set_contour_pen(const QPen& pen);
    QPen contour_pen() const;
    void set_contour_color(const QColor& color);
    QColor contour_color() const;
    void set_contour_width(double width);
    double contour_width() const;
    //! Writes each contour's level along its line, spaced, in the contour colour.
    void set_contour_labels_enabled(bool enabled);
    bool contour_labels_enabled() const;

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

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void auto_scale_y_changed(bool auto_scale_y);
    Q_SIGNAL void contour_levels_changed();
    Q_SIGNAL void contour_pen_changed(const QPen& pen);
    Q_SIGNAL void contour_labels_enabled_changed(bool enabled);
    Q_SIGNAL void autoscale_percentile_low_changed(double percentile);
    Q_SIGNAL void autoscale_percentile_high_changed(double percentile);

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

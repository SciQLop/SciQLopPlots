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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotAxisDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorGradientDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp"
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSignalBlocker>

namespace
{
SciQLopPlot* _owning_plot(QObject* axis)
{
    for (auto* p = axis ? axis->parent() : nullptr; p != nullptr; p = p->parent())
    {
        if (auto* plot = qobject_cast<SciQLopPlot*>(p))
            return plot;
    }
    return nullptr;
}

SciQLopColorMapBase* _colormap_for_axis(QObject* axis)
{
    if (auto* plot = _owning_plot(axis))
    {
        for (auto* p : plot->plottables())
        {
            if (auto* cm = qobject_cast<SciQLopColorMapBase*>(p))
                return cm;
        }
    }
    return nullptr;
}

SciQLopColorMap* _colormap_concrete_for_axis(QObject* axis)
{
    if (auto* plot = _owning_plot(axis))
    {
        for (auto* p : plot->plottables())
        {
            if (auto* cm = qobject_cast<SciQLopColorMap*>(p))
                return cm;
        }
    }
    return nullptr;
}
} // namespace


SciQLopPlotAxisInterface *SciQLopPlotAxisDelegate::axis() const
{
    return as_type<SciQLopPlotAxisInterface>(m_object);
}

SciQLopPlotColorScaleAxis *SciQLopPlotAxisDelegate::color_scale() const
{
    return as_type<SciQLopPlotColorScaleAxis>(m_object);
}

void SciQLopPlotAxisDelegate::addWidgetWithLabel(QWidget *widget, const QString &label)
{
    auto layout = new QHBoxLayout();
    auto label_widget = new QLabel(label);
    layout->addWidget(label_widget);
    layout->addWidget(widget);
    m_layout->addRow(layout);
}

SciQLopPlotAxisDelegate::SciQLopPlotAxisDelegate(SciQLopPlotAxisInterface* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto* ax = this->axis();

    auto* tickBox = new QGroupBox("Tick labels", this);
    auto* tickLayout = new QFormLayout(tickBox);

    auto* axis_visible_delegate = new BooleanDelegate(ax->tick_labels_visible(), tickBox);
    tickLayout->addRow("Visible", axis_visible_delegate);
    connect(axis_visible_delegate, &BooleanDelegate::value_changed, ax,
            &SciQLopPlotAxisInterface::set_tick_labels_visible);
    connect(ax, &SciQLopPlotAxisInterface::tick_labels_visible_changed,
            axis_visible_delegate, &BooleanDelegate::set_value);

    auto* tickFontDelegate
        = new FontDelegate(ax->tick_label_font(), ax->tick_label_color(), tickBox);
    tickLayout->addRow("Font", tickFontDelegate);
    connect(tickFontDelegate, &FontDelegate::fontChanged, ax,
            &SciQLopPlotAxisInterface::set_tick_label_font);
    connect(ax, &SciQLopPlotAxisInterface::tick_label_font_changed,
            tickFontDelegate, &FontDelegate::setFont);
    connect(tickFontDelegate, &FontDelegate::colorChanged, ax,
            &SciQLopPlotAxisInterface::set_tick_label_color);
    connect(ax, &SciQLopPlotAxisInterface::tick_label_color_changed,
            tickFontDelegate, &FontDelegate::setColor);

    m_layout->addRow(tickBox);

    if (!ax->is_time_axis())
    {
        auto* axis_log_delegate = new BooleanDelegate(ax->log(), this);
        connect(axis_log_delegate, &BooleanDelegate::value_changed, ax,
                &SciQLopPlotAxisInterface::set_log);
        connect(ax, &SciQLopPlotAxisInterface::log_changed, axis_log_delegate,
                &BooleanDelegate::set_value);
        addWidgetWithLabel(axis_log_delegate, "Log scale");
    }

    if (auto* color_scale = this->color_scale(); color_scale)
    {
        auto* color_scale_delegate
            = new ColorGradientDelegate(color_scale->color_gradient(), this);
        connect(color_scale_delegate, &ColorGradientDelegate::gradientChanged,
                color_scale, &SciQLopPlotColorScaleAxis::set_color_gradient);
        connect(color_scale, &SciQLopPlotColorScaleAxis::color_gradient_changed,
                color_scale_delegate, &ColorGradientDelegate::setGradient);
        addWidgetWithLabel(color_scale_delegate, "Color gradient");
    }

    // Label group: text + font/color picker.
    auto* labelBox = new QGroupBox("Label", this);
    auto* labelLayout = new QFormLayout(labelBox);

    auto* labelEdit = new QLineEdit(ax->label(), labelBox);
    labelLayout->addRow("Text", labelEdit);
    connect(labelEdit, &QLineEdit::editingFinished, ax,
            [labelEdit, ax]() { ax->set_label(labelEdit->text()); });
    connect(ax, &SciQLopPlotAxisInterface::label_changed, this,
            [labelEdit](const QString& s)
            {
                if (labelEdit->text() != s)
                    labelEdit->setText(s);
            });

    auto* labelFontDelegate = new FontDelegate(ax->label_font(), ax->label_color(), labelBox);
    labelLayout->addRow("Font", labelFontDelegate);
    connect(labelFontDelegate, &FontDelegate::fontChanged, ax,
            &SciQLopPlotAxisInterface::set_label_font);
    connect(ax, &SciQLopPlotAxisInterface::label_font_changed, labelFontDelegate,
            &FontDelegate::setFont);
    connect(labelFontDelegate, &FontDelegate::colorChanged, ax,
            &SciQLopPlotAxisInterface::set_label_color);
    connect(ax, &SciQLopPlotAxisInterface::label_color_changed, labelFontDelegate,
            &FontDelegate::setColor);

    m_layout->addRow(labelBox);

    // Range group (numeric, non-color-scale axes only).
    const bool show_range = !ax->is_time_axis() && (this->color_scale() == nullptr);
    if (show_range)
    {
        auto* rangeBox = new QGroupBox("Range", this);
        auto* rangeLayout = new QFormLayout(rangeBox);

        auto* minSpin = new QDoubleSpinBox(rangeBox);
        minSpin->setRange(-1e15, 1e15);
        minSpin->setDecimals(6);
        minSpin->setValue(ax->range().start());
        rangeLayout->addRow("Min", minSpin);

        auto* maxSpin = new QDoubleSpinBox(rangeBox);
        maxSpin->setRange(-1e15, 1e15);
        maxSpin->setDecimals(6);
        maxSpin->setValue(ax->range().stop());
        rangeLayout->addRow("Max", maxSpin);

        auto push_range = [minSpin, maxSpin, ax]()
        {
            ax->set_range(SciQLopPlotRange(minSpin->value(), maxSpin->value()));
        };
        connect(minSpin, &QDoubleSpinBox::editingFinished, this, push_range);
        connect(maxSpin, &QDoubleSpinBox::editingFinished, this, push_range);

        connect(ax, &SciQLopPlotAxisInterface::range_changed, this,
                [minSpin, maxSpin](SciQLopPlotRange r)
                {
                    QSignalBlocker bmin(minSpin);
                    QSignalBlocker bmax(maxSpin);
                    minSpin->setValue(r.start());
                    maxSpin->setValue(r.stop());
                });

        m_layout->addRow(rangeBox);
    }

    // Robust autoscale percentile: clamp the rescale range to a percentile of
    // the visible data, so a single outlier doesn't blow up the y-axis. Only
    // shown on **vertical** value axes — the rescale code pools graphs whose
    // y_axis() matches `this`, so the group would be a no-op on a key (x)
    // axis. Time axes don't autoscale on data extent.
    if (auto* concrete = as_type<SciQLopPlotAxis>(m_object);
        concrete && !ax->is_time_axis() && this->color_scale() == nullptr
        && ax->orientation() == Qt::Vertical)
    {
        auto* percentileBox = new QGroupBox("Autoscale percentile", this);
        auto* percentileLayout = new QFormLayout(percentileBox);

        auto* lowSpin = new QDoubleSpinBox(percentileBox);
        lowSpin->setRange(0., 100.);
        lowSpin->setDecimals(1);
        lowSpin->setSingleStep(0.5);
        lowSpin->setSuffix(" %");
        lowSpin->setValue(concrete->autoscale_percentile_low());
        percentileLayout->addRow("Low", lowSpin);
        connect(lowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double v)
                {
                    if (auto* a = as_type<SciQLopPlotAxis>(m_object))
                        a->set_autoscale_percentile_low(v);
                });

        auto* highSpin = new QDoubleSpinBox(percentileBox);
        highSpin->setRange(0., 100.);
        highSpin->setDecimals(1);
        highSpin->setSingleStep(0.5);
        highSpin->setSuffix(" %");
        highSpin->setValue(concrete->autoscale_percentile_high());
        percentileLayout->addRow("High", highSpin);
        connect(highSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double v)
                {
                    if (auto* a = as_type<SciQLopPlotAxis>(m_object))
                        a->set_autoscale_percentile_high(v);
                });

        m_layout->addRow(percentileBox);
    }

    // Color-scale percentile (z-axis only): proxies to the owning colormap's
    // SciQLopColorMapBase state. Lives here, not on the colormap node, because
    // users look for color-scale controls next to gradient/log/label.
    if (this->color_scale() != nullptr)
    {
        if (auto* cm = _colormap_for_axis(m_object))
        {
            auto* percentileBox = new QGroupBox("Autoscale percentile", this);
            auto* percentileLayout = new QFormLayout(percentileBox);

            auto* lowSpin = new QDoubleSpinBox(percentileBox);
            lowSpin->setRange(0., 100.);
            lowSpin->setDecimals(1);
            lowSpin->setSingleStep(0.5);
            lowSpin->setSuffix(" %");
            lowSpin->setValue(cm->autoscale_percentile_low());
            percentileLayout->addRow("Low", lowSpin);
            connect(lowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [this](double v)
                    {
                        if (auto* c = _colormap_for_axis(m_object))
                            c->set_autoscale_percentile_low(v);
                    });

            auto* highSpin = new QDoubleSpinBox(percentileBox);
            highSpin->setRange(0., 100.);
            highSpin->setDecimals(1);
            highSpin->setSingleStep(0.5);
            highSpin->setSuffix(" %");
            highSpin->setValue(cm->autoscale_percentile_high());
            percentileLayout->addRow("High", highSpin);
            connect(highSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [this](double v)
                    {
                        if (auto* c = _colormap_for_axis(m_object))
                            c->set_autoscale_percentile_high(v);
                    });

            m_layout->addRow(percentileBox);
        }
    }

    // Auto scale Y for colormaps: when this delegate is on the y2 axis of a
    // plot whose colormap drives the y range, expose the toggle here. Only
    // SciQLopColorMap (not Histogram2D) carries this knob.
    if (auto* plot = _owning_plot(m_object);
        plot && plot->y2_axis() == m_object)
    {
        if (auto* cm = _colormap_concrete_for_axis(m_object))
        {
            auto* check = new BooleanDelegate(cm->auto_scale_y(), this);
            check->setObjectName("auto_scale_y");
            m_layout->addRow("Auto scale Y", check);
            connect(check, &BooleanDelegate::value_changed, this,
                    [this](bool v)
                    {
                        if (auto* c = _colormap_concrete_for_axis(m_object))
                            c->set_auto_scale_y(v);
                    });
            connect(cm, &SciQLopColorMap::auto_scale_y_changed, check,
                    &BooleanDelegate::set_value);
        }
    }

    append_inspector_extensions();
}


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
#include "SciQLopPlots/Inspector/PropertyDelegates.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorGradientDelegate.hpp"

REGISTER_DELEGATE(SciQLopPlotAxisDelegate)


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
    auto ax = this->axis();
    auto axis_visible_delegate = new BooleanDelegate(ax->tick_labels_visible(), this);
    connect(axis_visible_delegate, &BooleanDelegate::value_changed, ax, &SciQLopPlotAxisInterface::set_tick_labels_visible);
    connect(ax, &SciQLopPlotAxisInterface::tick_labels_visible_changed, axis_visible_delegate, &BooleanDelegate::set_value);
    addWidgetWithLabel(axis_visible_delegate, "Tick labels visible");

    if(!ax->is_time_axis())
    {
        auto axis_log_delegate = new BooleanDelegate(ax->log(), this);
        connect(axis_log_delegate, &BooleanDelegate::value_changed, ax, &SciQLopPlotAxisInterface::set_log);
        connect(ax, &SciQLopPlotAxisInterface::log_changed, axis_log_delegate, &BooleanDelegate::set_value);
        addWidgetWithLabel(axis_log_delegate, "Log scale");
    }

    if (auto color_scale = this->color_scale(); color_scale)
    {
       auto color_scale_delegate = new ColorGradientDelegate(color_scale->color_gradient(), this);
       connect(color_scale_delegate, &ColorGradientDelegate::gradientChanged, color_scale, &SciQLopPlotColorScaleAxis::set_color_gradient);
       connect(color_scale, &SciQLopPlotColorScaleAxis::color_gradient_changed, color_scale_delegate, &ColorGradientDelegate::setGradient);
       addWidgetWithLabel(color_scale_delegate, "Color gradient");
    }

}


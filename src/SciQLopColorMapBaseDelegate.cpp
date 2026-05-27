/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>

SciQLopColorMapBase* SciQLopColorMapBaseDelegate::color_map_base() const
{
    return as_type<SciQLopColorMapBase>(m_object);
}

SciQLopColorMapBaseDelegate::SciQLopColorMapBaseDelegate(SciQLopColorMapBase* object,
                                                         QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    // Gradient is owned by the color-scale axis (SciQLopPlotColorScaleAxis) and
    // edited from the z-axis delegate. Don't duplicate it here.
    auto* percentileBox = new QGroupBox("Color scale percentile", this);
    auto* percentileLayout = new QFormLayout(percentileBox);

    auto* lowSpin = new QDoubleSpinBox(percentileBox);
    lowSpin->setRange(0., 100.);
    lowSpin->setDecimals(1);
    lowSpin->setSingleStep(0.5);
    lowSpin->setSuffix(" %");
    lowSpin->setValue(object->autoscale_percentile_low());
    percentileLayout->addRow("Low", lowSpin);
    connect(lowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v)
            {
                if (auto* cm = color_map_base())
                    cm->set_autoscale_percentile_low(v);
            });

    auto* highSpin = new QDoubleSpinBox(percentileBox);
    highSpin->setRange(0., 100.);
    highSpin->setDecimals(1);
    highSpin->setSingleStep(0.5);
    highSpin->setSuffix(" %");
    highSpin->setValue(object->autoscale_percentile_high());
    percentileLayout->addRow("High", highSpin);
    connect(highSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v)
            {
                if (auto* cm = color_map_base())
                    cm->set_autoscale_percentile_high(v);
            });

    m_layout->addRow(percentileBox);
}

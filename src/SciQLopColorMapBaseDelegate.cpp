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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QSpinBox>

SciQLopColorMapBase* SciQLopColorMapBaseDelegate::color_map_base() const
{
    return as_type<SciQLopColorMapBase>(m_object);
}

SciQLopColorMapBaseDelegate::SciQLopColorMapBaseDelegate(SciQLopColorMapBase* object,
                                                         QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto* visibleCheck = new BooleanDelegate("Visible", object->visible(), this);
    m_layout->addRow(visibleCheck);
    connect(visibleCheck, &BooleanDelegate::value_changed, object,
            &SciQLopPlottableInterface::set_visible);
    connect(object, &SciQLopPlottableInterface::visible_changed, visibleCheck,
            [visibleCheck](bool v)
            {
                QSignalBlocker blocker(visibleCheck);
                visibleCheck->set_value(v);
            });
    // Color-scale controls (gradient, autoscale percentile) live on the
    // "Color Scale" (z) axis node. Product-specific knobs are added by derived
    // delegates (Binning/normalization).
    add_contours_box(object);
}

void SciQLopColorMapBaseDelegate::add_contours_box(SciQLopColorMapBase* object)
{
    auto* contoursBox = new QGroupBox("Contours", this);
    auto* contoursLayout = new QFormLayout(contoursBox);

    auto* countSpin = new QSpinBox(contoursBox);
    countSpin->setRange(0, 50);
    countSpin->setValue(object->auto_contour_level_count());
    contoursLayout->addRow("Auto level count", countSpin);
    connect(countSpin, QOverload<int>::of(&QSpinBox::valueChanged), object,
            [object](int v) { object->set_auto_contour_levels(v); });

    auto* colorWidget = new ColorDelegate(object->contour_color(), contoursBox);
    contoursLayout->addRow("Color", colorWidget);
    connect(colorWidget, &ColorDelegate::colorChanged, object,
            [object](const QColor& c) { object->set_contour_color(c); });

    auto* widthSpin = new QDoubleSpinBox(contoursBox);
    widthSpin->setRange(0.1, 10.0);
    widthSpin->setDecimals(1);
    widthSpin->setSingleStep(0.1);
    widthSpin->setValue(object->contour_width());
    widthSpin->setToolTip("Not yet implemented: contour lines are always drawn at a fixed hairline width.");
    contoursLayout->addRow("Width", widthSpin);
    connect(widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), object,
            [object](double w) { object->set_contour_width(w); });

    auto* labelsCheck = new BooleanDelegate(object->contour_labels_enabled(), contoursBox);
    contoursLayout->addRow("Show labels", labelsCheck);
    connect(labelsCheck, &BooleanDelegate::value_changed, object,
            &SciQLopColorMapBase::set_contour_labels_enabled);

    m_layout->addRow(contoursBox);

    // Reverse path: model -> widgets, signal-blocked to avoid loops.
    connect(object, &SciQLopColorMapBase::contour_levels_changed, this,
            [countSpin, object]()
            {
                QSignalBlocker b(countSpin);
                countSpin->setValue(object->auto_contour_level_count());
            });
    connect(object, &SciQLopColorMapBase::contour_pen_changed, this,
            [colorWidget, widthSpin](const QPen& pen)
            {
                colorWidget->setColor(pen.color());
                QSignalBlocker b(widthSpin);
                widthSpin->setValue(pen.widthF());
            });
    connect(object, &SciQLopColorMapBase::contour_labels_enabled_changed, labelsCheck,
            &BooleanDelegate::set_value);
}


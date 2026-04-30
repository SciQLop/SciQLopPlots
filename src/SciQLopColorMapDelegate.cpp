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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QSpinBox>

SciQLopColorMap* SciQLopColorMapDelegate::colorMap() const
{
    return as_type<SciQLopColorMap>(m_object);
}

SciQLopColorMapDelegate::SciQLopColorMapDelegate(SciQLopColorMap* object, QWidget* parent)
        : SciQLopColorMapBaseDelegate(object, parent)
{
    auto* auto_scale = new BooleanDelegate(object->auto_scale_y(), this);
    m_layout->addRow("Auto scale Y", auto_scale);
    connect(auto_scale, &BooleanDelegate::value_changed, object,
            &SciQLopColorMap::set_auto_scale_y);
    connect(object, &SciQLopColorMap::auto_scale_y_changed, auto_scale,
            &BooleanDelegate::set_value);

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
    contoursLayout->addRow("Width", widthSpin);
    connect(widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), object,
            [object](double w) { object->set_contour_width(w); });

    auto* labelsCheck = new BooleanDelegate(object->contour_labels_enabled(), contoursBox);
    contoursLayout->addRow("Show labels", labelsCheck);
    connect(labelsCheck, &BooleanDelegate::value_changed, object,
            &SciQLopColorMap::set_contour_labels_enabled);

    m_layout->addRow(contoursBox);

    // Reverse path: model -> widgets, signal-blocked to avoid loops.
    connect(object, &SciQLopColorMap::contour_levels_changed, this,
            [countSpin, object]()
            {
                QSignalBlocker b(countSpin);
                countSpin->setValue(object->auto_contour_level_count());
            });
    connect(object, &SciQLopColorMap::contour_pen_changed, this,
            [colorWidget, widthSpin](const QPen& pen)
            {
                colorWidget->setColor(pen.color());
                QSignalBlocker b(widthSpin);
                widthSpin->setValue(pen.widthF());
            });
    connect(object, &SciQLopColorMap::contour_labels_enabled_changed, labelsCheck,
            &BooleanDelegate::set_value);

    append_inspector_extensions();
}

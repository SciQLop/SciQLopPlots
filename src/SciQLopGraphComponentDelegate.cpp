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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphComponentDelegate.hpp"

#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LineDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>

SciQLopGraphComponentInterface* SciQLopGraphComponentDelegate::component() const
{
    return as_type<SciQLopGraphComponentInterface>(m_object);
}

SciQLopGraphComponentDelegate::SciQLopGraphComponentDelegate(SciQLopGraphComponentInterface* object,
                                                             QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    m_lineDelegate = new LineDelegate(object->pen(), object->line_style(),
                                      object->marker_shape(), this);
    m_layout->addWidget(m_lineDelegate);
    connect(m_lineDelegate, &LineDelegate::penChanged, object,
            &SciQLopGraphComponentInterface::set_pen);
    connect(m_lineDelegate, &LineDelegate::styleChanged, object,
            &SciQLopGraphComponentInterface::set_line_style);
    connect(m_lineDelegate, &LineDelegate::markerShapeChanged, object,
            &SciQLopGraphComponentInterface::set_marker_shape);

    auto* markerBox = new QGroupBox("Marker", this);
    auto* markerLayout = new QFormLayout(markerBox);

    auto* penColor = new ColorDelegate(object->marker_pen().color(), markerBox);
    markerLayout->addRow("Pen color", penColor);
    connect(penColor, &ColorDelegate::colorChanged, this,
            [object](const QColor& c)
            {
                auto pen = object->marker_pen();
                pen.setColor(c);
                object->set_marker_pen(pen);
            });

    auto* sizeSpin = new QDoubleSpinBox(markerBox);
    sizeSpin->setRange(1.0, 30.0);
    sizeSpin->setDecimals(1);
    sizeSpin->setSingleStep(0.5);
    sizeSpin->setValue(object->marker_size());
    markerLayout->addRow("Size", sizeSpin);
    connect(sizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), object,
            &SciQLopGraphComponentInterface::set_marker_size);

    m_layout->addWidget(markerBox);

    // Reverse path
    connect(object, &SciQLopGraphComponentInterface::marker_pen_changed, this,
            [penColor](const QPen& pen) { penColor->setColor(pen.color()); });
    connect(object, &SciQLopGraphComponentInterface::marker_size_changed, this,
            [sizeSpin](qreal s)
            {
                QSignalBlocker b(sizeSpin);
                sizeSpin->setValue(s);
            });

    append_inspector_extensions();
}

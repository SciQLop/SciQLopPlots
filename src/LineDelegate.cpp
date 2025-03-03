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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LineDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/MarkerDelegate.hpp"
#include "SciQLopPlots/enums.hpp"
#include "magic_enum/magic_enum_utility.hpp"

#include <QComboBox>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QSpinBox>
#include <fmt/core.h>

LineDelegate::LineDelegate(QPen pen, GraphLineStyle style, GraphMarkerShape marker_shape,
                           QWidget* parent)
        : QWidget(parent)
{
    m_pen = pen;
    m_layout = new QFormLayout();
    setLayout(m_layout);

    auto width = new QSpinBox();
    m_layout->addRow("Width", width);
    width->setValue(pen.width());
    connect(width, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value)
            {
                m_pen.setWidth(value);
                emit widthChanged(value);
                emit penChanged(m_pen);
            });

    auto style_qcb = new QComboBox();
    m_layout->addRow("Style", style_qcb);
    magic_enum::enum_for_each<GraphLineStyle>(
        [style_qcb](GraphLineStyle line_style)
        {
            style_qcb->addItem(QIcon(QString::fromStdString(fmt::format(
                                   ":/LineStyles/{}.png", magic_enum::enum_name(line_style)))),
                               to_string(line_style), QVariant::fromValue(line_style));
        });
    style_qcb->setCurrentIndex(style_qcb->findData(QVariant::fromValue(style)));
    connect(style_qcb, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, style_qcb](int index)
            { emit styleChanged(style_qcb->itemData(index).value<GraphLineStyle>()); });

    auto color = new ColorDelegate(pen.color());
    m_layout->addRow("Color", color);
    connect(color, &ColorDelegate::colorChanged, this,
            [this](const QColor& color)
            {
                m_pen.setColor(color);
                emit colorChanged(color);
                emit penChanged(m_pen);
            });

    auto marker = new MarkerDelegate(marker_shape);
    m_layout->addRow("Marker", marker);
    connect(marker, &MarkerDelegate::markerShapeChanged, this,
            [this](GraphMarkerShape shape) { emit this->markerShapeChanged(shape); });
}

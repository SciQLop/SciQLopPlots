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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorGradientDelegate.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include "magic_enum/magic_enum_utility.hpp"
#include "qcustomplot.h"
#include <QPainter>


QIcon icon(ColorGradient gradient)
{
    auto gradient_qcp = QCPColorGradient(to_qcp(gradient));
    QPixmap pixmap(128, 128);
    QPainter painter(&pixmap);
    for (auto i = 0; i < pixmap.width(); i++)
    {
        painter.fillRect(i, 0, 1, pixmap.height(), gradient_qcp.color(double(i) / pixmap.width(), QCPRange(0, 1)));
    }
    return QIcon(pixmap);
}

ColorGradientDelegate::ColorGradientDelegate(ColorGradient gradient, QWidget* parent)
        : QComboBox(parent)
{
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
            { emit gradientChanged(this->itemData(index).value<ColorGradient>()); });

    magic_enum::enum_for_each<ColorGradient>(
        [this](ColorGradient gradient) {
            addItem(icon(gradient),
              to_string(gradient), QVariant::fromValue(gradient)); });
    setGradient(gradient);
}

void ColorGradientDelegate::setGradient(ColorGradient gradient)
{
    m_gradient = gradient;
    setCurrentIndex(findData(QVariant::fromValue(gradient)));
    emit gradientChanged(gradient);
}

ColorGradient ColorGradientDelegate::gradient() const
{
    return m_gradient;
}

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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"

ColorDelegate::ColorDelegate(QColor color, QWidget* parent) : QPushButton(parent)
{
    setAutoFillBackground(true);
    setFlat(true);
    setColor(color);
    connect(this, &QPushButton::clicked, this, &ColorDelegate::pick_color);
}

void ColorDelegate::setColor(const QColor& color)
{
    if (m_color != color)
    {
        m_color = color;
        QPalette palette;
        palette.setColor(QPalette::Button, m_color);
        setPalette(palette);
        emit colorChanged(m_color);
    }
}

QColor ColorDelegate::color() const
{
    return m_color;
}

void ColorDelegate::pick_color()
{
    QColorDialog dialog;
    dialog.setCurrentColor(m_color);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (dialog.selectedColor().isValid())
        {
            setColor(dialog.selectedColor());
        }
    }
}

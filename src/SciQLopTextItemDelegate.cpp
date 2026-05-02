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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp"
#include "SciQLopPlots/Items/SciQLopTextItem.hpp"

#include <QLineEdit>

SciQLopTextItem* SciQLopTextItemDelegate::item() const
{
    return as_type<SciQLopTextItem>(m_object);
}

SciQLopTextItemDelegate::SciQLopTextItemDelegate(SciQLopTextItem* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto* it = item();

    auto* textEdit = new QLineEdit(it->text(), this);
    m_layout->addRow("Text", textEdit);
    connect(textEdit, &QLineEdit::editingFinished, it,
            [textEdit, it]() { it->set_text(textEdit->text()); });

    auto* fontDelegate = new FontDelegate(it->font(), it->color(), this);
    m_layout->addRow("Font", fontDelegate);
    connect(fontDelegate, &FontDelegate::fontChanged, it, &SciQLopTextItem::set_font);
    connect(fontDelegate, &FontDelegate::colorChanged, it, &SciQLopTextItem::set_color);
    // SciQLopTextItem has no font_changed/color_changed signals (the surface is
    // setter-only); the model->widget reverse path is therefore omitted.

    append_inspector_extensions();
}

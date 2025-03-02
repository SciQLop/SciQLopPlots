/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/MarkerDelegate.hpp"
#include "magic_enum/magic_enum_utility.hpp"
#include <QComboBox>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QSpinBox>
#include <QStringLiteral>
#include <fmt/core.h>

MarkerDelegate::MarkerDelegate(GraphMarkerShape shape, QWidget* parent) : QComboBox(parent)
{
    setItemDelegate(new MarkerItemDelegate(this));
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
            { emit markerShapeChanged(this->itemData(index).value<GraphMarkerShape>()); });

    magic_enum::enum_for_each<GraphMarkerShape>(
        [this](GraphMarkerShape shape) { addItem(to_string(shape), QVariant::fromValue(shape)); });
    setMarkerShape(shape);
}

void MarkerDelegate::setMarkerShape(GraphMarkerShape shape)
{
    setCurrentIndex(findData(QVariant::fromValue(shape)));
    emit markerShapeChanged(shape);
}

GraphMarkerShape MarkerDelegate::markerShape() const
{
    return currentData().value<GraphMarkerShape>();
}

inline QSize expanded(QSize size, int w, int h)
{
    return QSize(size.width() + 2 * w, size.height() + 2 * h);
}

QSize MarkerItemDelegate::graphic_item_size_hint() const
{
    auto height = text_size_hint("T").height();
    return QSize(height, height);
}

void MarkerItemDelegate::paint_graphic_item(QPainter* painter, const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    auto data = index.data(Qt::UserRole);
    if (data.canConvert<GraphMarkerShape>())
    {
        auto resourcename = QString::fromStdString(
            fmt::format(":/Markers/{}.png", magic_enum::enum_name(data.value<GraphMarkerShape>())));
        auto sh = graphic_item_size_hint();
        auto rect = option.rect;
        if (QFile::exists(resourcename))
        {
            auto image = QIcon(resourcename).pixmap(sh);
            painter->save();
            painter->translate(rect.x() + rect.width() - sh.width(), rect.y() + this->_v_margin);
            painter->drawPixmap(0, 0, image);
            painter->restore();
        }
    }
}

MarkerItemDelegate::MarkerItemDelegate(QObject* parent) : StyledItemDelegate(parent) { }

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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/StyledItemDelegate.hpp"
#include <QPainter>

StyledItemDelegate::StyledItemDelegate(QObject* parent) : QStyledItemDelegate(parent) { }

inline QSize expanded(QSize size, int w, int h)
{
    return QSize(size.width() + 2 * w, size.height() + 2 * h);
}

QSize StyledItemDelegate::text_size_hint(const QString& text) const
{
    return expanded(qobject_cast<QWidget*>(parent())->fontMetrics().size(0, text), _h_margin,
                    _v_margin);
}

void StyledItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    if (index.isValid())
    {
        auto text = option.text;
        auto tx = text_size_hint(text);
        auto rect = option.rect;
        if (option.state & QStyle::State_Selected)
        {
            painter->fillRect(rect, option.palette.highlight());
        }
        painter->save();
        painter->translate(rect.x() + _h_margin,
                           rect.y() - rect.height() / 2 + tx.height() + _v_margin);
        if (option.state & QStyle::State_Selected)
        {
            painter->setPen(option.palette.highlightedText().color());
        }
        painter->drawText(0, 0, index.data().toString());
        painter->restore();
        painter->resetTransform();
        paint_graphic_item(painter, option, index);
    }
    else
    {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize StyledItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    if (index.isValid())
    {
        auto tx = text_size_hint(index.data().toString());
        auto sh = graphic_item_size_hint();
        return QSize(tx.width() + sh.width(), std::max(tx.height(), sh.height()));
    }
    return QStyledItemDelegate::sizeHint(option, index);
}

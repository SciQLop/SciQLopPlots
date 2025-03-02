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
#pragma once


#include <QStyledItemDelegate>

/**
 * @brief The StyledItemDelegate class
 * This class is a base class for all item delegates that display a text and a graphic item in a
 * QComboBox. The text is displayed on the left and the graphic item on the right.
 */

class StyledItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

protected:
    QSize text_size_hint(const QString& text) const;
    virtual QSize graphic_item_size_hint() const = 0;
    virtual void paint_graphic_item(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const = 0;
    int _h_margin = 5;
    int _v_margin = 5;

public:
    StyledItemDelegate(QObject* parent = nullptr);
    virtual ~StyledItemDelegate() = default;

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option,
                           const QModelIndex& index) const override;
};

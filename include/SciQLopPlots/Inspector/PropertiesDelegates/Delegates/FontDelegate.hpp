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
#pragma once

#include <QColor>
#include <QFont>
#include <QPointer>
#include <QPushButton>

class QWidget;
class QFontComboBox;
class QSpinBox;
class QToolButton;
class ColorDelegate;

class FontDelegate : public QPushButton
{
    Q_OBJECT

    QFont m_font;
    QColor m_color;

    QPointer<QWidget> m_popup;
    QFontComboBox* m_family = nullptr;
    QSpinBox* m_size = nullptr;
    QToolButton* m_bold = nullptr;
    QToolButton* m_italic = nullptr;
    ColorDelegate* m_color_picker = nullptr;

public:
    FontDelegate(const QFont& font, const QColor& color, QWidget* parent = nullptr);
    virtual ~FontDelegate() = default;

    QFont font() const;
    QColor color() const;

public:
    // Shadows QWidget::setFont; only this slot tracks m_font and emits.
    // Stylesheet relayouts that call the inherited setFont bypass us.
    Q_SLOT void setFont(const QFont& font);
    Q_SLOT void setColor(const QColor& color);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void fontChanged(const QFont& font);
    Q_SIGNAL void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Q_SLOT void toggle_popup();
    void ensure_popup();
    void sync_popup_widgets();
};

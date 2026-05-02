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
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"

#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr int MIN_FONT_SIZE = 4;
constexpr int MAX_FONT_SIZE = 72;
constexpr int DEFAULT_POINT_SIZE = 10;
}

FontDelegate::FontDelegate(const QFont& font, const QColor& color, QWidget* parent)
        : QPushButton(parent)
        , m_font(font)
        , m_color(color)
{
    setAutoFillBackground(false);
    setFlat(true);
    setMinimumWidth(48);
    connect(this, &QPushButton::clicked, this, &FontDelegate::toggle_popup);
}

QFont FontDelegate::font() const
{
    return m_font;
}

QColor FontDelegate::color() const
{
    return m_color;
}

void FontDelegate::setFont(const QFont& font)
{
    if (m_font != font)
    {
        m_font = font;
        if (m_popup)
            sync_popup_widgets();
        update();
        Q_EMIT fontChanged(m_font);
    }
}

void FontDelegate::setColor(const QColor& color)
{
    if (m_color != color)
    {
        m_color = color;
        if (m_color_picker)
        {
            QSignalBlocker b(m_color_picker);
            m_color_picker->setColor(color);
        }
        update();
        Q_EMIT colorChanged(m_color);
    }
}

void FontDelegate::toggle_popup()
{
    ensure_popup();
    if (m_popup->isVisible())
    {
        m_popup->hide();
    }
    else
    {
        sync_popup_widgets();
        m_popup->move(mapToGlobal(QPoint(0, height())));
        m_popup->show();
    }
}

void FontDelegate::ensure_popup()
{
    if (m_popup)
        return;

    m_popup = new QWidget(this, Qt::Popup);
    auto* outer = new QVBoxLayout(m_popup);

    auto* font_row = new QHBoxLayout();
    m_family = new QFontComboBox(m_popup);
    m_family->setCurrentFont(m_font);
    font_row->addWidget(m_family);

    m_size = new QSpinBox(m_popup);
    m_size->setRange(MIN_FONT_SIZE, MAX_FONT_SIZE);
    m_size->setValue(m_font.pointSize() > 0 ? m_font.pointSize() : DEFAULT_POINT_SIZE);
    m_size->setSuffix(" pt");
    font_row->addWidget(m_size);
    outer->addLayout(font_row);

    auto* style_row = new QHBoxLayout();
    m_bold = new QToolButton(m_popup);
    m_bold->setText("B");
    m_bold->setCheckable(true);
    m_bold->setChecked(m_font.bold());
    QFont bold_face = m_bold->font();
    bold_face.setBold(true);
    m_bold->setFont(bold_face);
    style_row->addWidget(m_bold);

    m_italic = new QToolButton(m_popup);
    m_italic->setText("I");
    m_italic->setCheckable(true);
    m_italic->setChecked(m_font.italic());
    QFont italic_face = m_italic->font();
    italic_face.setItalic(true);
    m_italic->setFont(italic_face);
    style_row->addWidget(m_italic);
    style_row->addStretch();
    outer->addLayout(style_row);

    auto* color_row = new QHBoxLayout();
    color_row->addWidget(new QLabel("Color"));
    m_color_picker = new ColorDelegate(m_color, m_popup);
    color_row->addWidget(m_color_picker);
    outer->addLayout(color_row);

    auto push_font_from_widgets = [this]()
    {
        QFont f = m_family->currentFont();
        f.setPointSize(m_size->value());
        f.setBold(m_bold->isChecked());
        f.setItalic(m_italic->isChecked());
        setFont(f);
    };

    connect(m_family, &QFontComboBox::currentFontChanged, this, push_font_from_widgets);
    connect(m_size, QOverload<int>::of(&QSpinBox::valueChanged), this, push_font_from_widgets);
    connect(m_bold, &QToolButton::toggled, this, push_font_from_widgets);
    connect(m_italic, &QToolButton::toggled, this, push_font_from_widgets);

    connect(m_color_picker, &ColorDelegate::colorChanged, this, &FontDelegate::setColor);
}

void FontDelegate::sync_popup_widgets()
{
    if (!m_popup)
        return;
    QSignalBlocker bf(m_family);
    QSignalBlocker bs(m_size);
    QSignalBlocker bb(m_bold);
    QSignalBlocker bi(m_italic);
    QSignalBlocker bc(m_color_picker);
    m_family->setCurrentFont(m_font);
    m_size->setValue(m_font.pointSize() > 0 ? m_font.pointSize() : DEFAULT_POINT_SIZE);
    m_bold->setChecked(m_font.bold());
    m_italic->setChecked(m_font.italic());
    m_color_picker->setColor(m_color);
}

void FontDelegate::paintEvent(QPaintEvent* event)
{
    QPushButton::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(m_color);
    QFont preview = m_font;
    if (preview.pointSize() <= 0)
        preview.setPointSize(DEFAULT_POINT_SIZE);
    painter.setFont(preview);
    painter.drawText(rect(), Qt::AlignCenter, "Aa");
}

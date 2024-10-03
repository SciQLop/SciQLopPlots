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
#include "qcustomplot.h"
#include <QPainter>

static QCPColorGradient ColorGradien_to_QCPColorGradient(ColorGradient gradient)
{
    switch (gradient)
    {
        case ColorGradient::Grayscale:
            return QCPColorGradient::gpGrayscale;
        case ColorGradient::Hot:
            return QCPColorGradient::gpHot;
        case ColorGradient::Cold:
            return QCPColorGradient::gpCold;
        case ColorGradient::Night:
            return QCPColorGradient::gpNight;
        case ColorGradient::Candy:
            return QCPColorGradient::gpCandy;
        case ColorGradient::Geography:
            return QCPColorGradient::gpGeography;
        case ColorGradient::Ion:
            return QCPColorGradient::gpIon;
        case ColorGradient::Thermal:
            return QCPColorGradient::gpThermal;
        case ColorGradient::Polar:
            return QCPColorGradient::gpPolar;
        case ColorGradient::Spectrum:
            return QCPColorGradient::gpSpectrum;
        case ColorGradient::Jet:
            return QCPColorGradient::gpJet;
        case ColorGradient::Hues:
            return QCPColorGradient::gpHues;
    }
    return QCPColorGradient::gpJet;
}

inline QSize expanded(QSize size, int w, int h)
{
    return QSize(size.width() + 2 * w, size.height() + 2 * h);
}

ColorGradientDelegate::ColorGradientDelegate(ColorGradient gradient, QWidget* parent)
        : QComboBox(parent)
{
    setItemDelegate(new ColorGradientItemDelegate(this));
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
            { emit gradientChanged(this->itemData(index).value<ColorGradient>()); });
    addItem(color_gradient_to_string(ColorGradient::Grayscale),
            QVariant::fromValue(ColorGradient::Grayscale));
    addItem(color_gradient_to_string(ColorGradient::Hot), QVariant::fromValue(ColorGradient::Hot));
    addItem(color_gradient_to_string(ColorGradient::Cold),
            QVariant::fromValue(ColorGradient::Cold));
    addItem(color_gradient_to_string(ColorGradient::Night),
            QVariant::fromValue(ColorGradient::Night));
    addItem(color_gradient_to_string(ColorGradient::Candy),
            QVariant::fromValue(ColorGradient::Candy));
    addItem(color_gradient_to_string(ColorGradient::Geography),
            QVariant::fromValue(ColorGradient::Geography));
    addItem(color_gradient_to_string(ColorGradient::Ion), QVariant::fromValue(ColorGradient::Ion));
    addItem(color_gradient_to_string(ColorGradient::Thermal),
            QVariant::fromValue(ColorGradient::Thermal));
    addItem(color_gradient_to_string(ColorGradient::Polar),
            QVariant::fromValue(ColorGradient::Polar));
    addItem(color_gradient_to_string(ColorGradient::Spectrum),
            QVariant::fromValue(ColorGradient::Spectrum));
    addItem(color_gradient_to_string(ColorGradient::Jet), QVariant::fromValue(ColorGradient::Jet));
    addItem(color_gradient_to_string(ColorGradient::Hues),
            QVariant::fromValue(ColorGradient::Hues));
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

QSize ColorGradientItemDelegate::_text_size_hint(const QString& text) const
{
    return expanded(qobject_cast<QWidget*>(parent())->fontMetrics().size(0, text), _h_margin,
                    _v_margin);
}

QSize ColorGradientItemDelegate::_gradient_size_hint() const
{
    return _text_size_hint("000000000");
}

ColorGradientItemDelegate::ColorGradientItemDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
}

void ColorGradientItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    if (index.isValid())
    {
        auto data = index.data(Qt::UserRole);
        if (data.canConvert<ColorGradient>())
        {
            auto gradient = ColorGradien_to_QCPColorGradient(data.value<ColorGradient>());
            auto sh = _gradient_size_hint();
            auto tx = _text_size_hint(index.data().toString());
            auto rect = option.rect;
            if (option.state & QStyle::State_Selected)
            {
                painter->fillRect(rect, option.palette.highlight());
            }
            rect.adjust(+5, 0, -5, 0);
            painter->save();
            painter->translate(rect.x(), rect.y() + tx.height());
            if (option.state & QStyle::State_Selected)
            {
                painter->setPen(option.palette.highlightedText().color());
            }
            painter->drawText(0, 0, index.data().toString());
            painter->restore();
            painter->resetTransform();
            painter->save();
            painter->translate(rect.x() + tx.width() + _h_margin, rect.y());
            auto width = rect.width() - tx.width() - _h_margin - _h_margin;
            for (auto i = 0; i < width; i++)
            {
                painter->setPen(gradient.color(double(i) / width, QCPRange(0, 1)));
                painter->drawLine(i, 0, i, sh.height());
            }
            painter->restore();
        }
        else
        {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
    /*      if index.isValid():
              data = index.data(Qt.UserRole)
              if isinstance(data, QCPColorGradient.GradientPreset):
                  gradient = QCPColorGradient(data)
                  sh = self._gradient_size_hint()
                  tx = self._text_size_hint(index.data())
                  rect: QRect = option.rect
                  if option.state & QStyle.StateFlag.State_Selected ==
       QStyle.StateFlag.State_Selected: painter.fillRect(rect, option.palette.highlight())
                  rect.adjust(+5, 0, -5, 0)
                  painter.save()
                  painter.translate(rect.x(), rect.y() + tx.height())
                  if option.state & QStyle.StateFlag.State_Selected ==
       QStyle.StateFlag.State_Selected: painter.setPen(option.palette.highlightedText().color())
                  painter.drawText(0, 0, index.data())
                  painter.restore()
                  painter.resetTransform()
                  painter.save()
                  painter.translate(rect.x() + tx.width() + self._h_margin, rect.y())
                  width = rect.width() - tx.width() - self._h_margin - self._h_margin
                  for i in range(width):
                      painter.setPen(gradient.color(i / width, QCPRange(0, 1)))
                      painter.drawLine(i, 0, i, sh.height())
                  painter.restore()
              else:
                  super().paint(painter, option, index)
          else:
              super().paint(painter, option, index)
      */
}

QSize ColorGradientItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const
{
    if (index.isValid())
    {
        auto data = index.data(Qt::UserRole);
        if (data.canConvert<ColorGradient>())
        {
            auto tx = _text_size_hint(index.data().toString());
            auto sh = _gradient_size_hint();
            return QSize(tx.width() + sh.width(), std::max(tx.height(), sh.height()));
        }
        else
        {
            return QStyledItemDelegate::sizeHint(option, index);
        }
    }
    return QStyledItemDelegate::sizeHint(option, index);
}

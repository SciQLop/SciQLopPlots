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
#pragma once

#include "SciQLopPlots/enums.hpp"

#include <QColor>
#include <QColorDialog>
#include <QFormLayout>
#include <QPen>
#include <QWidget>


class LineDelegate : public QWidget
{
    Q_OBJECT

    QPen m_pen;
    QFormLayout* m_layout;
    GraphLineStyle m_style;

public:
    LineDelegate(QPen pen, GraphLineStyle style,GraphMarkerShape marker_shape, QWidget* parent = nullptr);
    virtual ~LineDelegate() = default;


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void colorChanged(const QColor& color);
    Q_SIGNAL void widthChanged(int width);
    Q_SIGNAL void styleChanged(GraphLineStyle style);
    Q_SIGNAL void penChanged(const QPen& pen);
    Q_SIGNAL void markerShapeChanged(GraphMarkerShape shape);

};

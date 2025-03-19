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

#include <QColor>
#include <QColorDialog>

#include "SciQLopPlots/enums.hpp"
#include <QComboBox>
#include <QWidget>

class MarkerDelegate : public QComboBox
{
    Q_OBJECT
    GraphMarkerShape m_shape;

public:
    MarkerDelegate(GraphMarkerShape shape, QWidget* parent = nullptr);
    virtual ~MarkerDelegate() = default;

    void setMarkerShape(GraphMarkerShape shape);
    GraphMarkerShape markerShape() const;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void markerShapeChanged(GraphMarkerShape shape);

};

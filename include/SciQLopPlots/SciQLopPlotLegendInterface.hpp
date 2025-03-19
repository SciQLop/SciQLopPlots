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

#include "SciQLopPlots/Debug.hpp"
#include <QPointF>
#include <QObject>

class SciQLopPlotLegendInterface : public QObject
{
    Q_OBJECT

public:
    SciQLopPlotLegendInterface(QObject* parent = nullptr) : QObject(parent) { }

    virtual ~SciQLopPlotLegendInterface() = default;

    inline virtual bool is_visible() const
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    inline virtual void set_visible(bool visible)
    {
        WARN_ABSTRACT_METHOD;
    }

    inline virtual QPointF position() const
    {
        WARN_ABSTRACT_METHOD;
        return QPointF();
    }

    inline virtual void set_position(const QPointF&)
    {
        WARN_ABSTRACT_METHOD;
    }


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void visibility_changed(bool value);
    Q_SIGNAL void position_changed(const QPointF& value);

};

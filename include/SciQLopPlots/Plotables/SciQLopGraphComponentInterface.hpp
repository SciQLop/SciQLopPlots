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

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"

#include <QColor>
#include <QList>
#include <QObject>
#include <QPen>

class SciQLopGraphComponentInterface : public QObject
{
    SciQLopPlotRange m_range;
    Q_OBJECT

public:
    Q_PROPERTY(bool selected READ selected WRITE set_selected NOTIFY selection_changed)

    SciQLopGraphComponentInterface(QObject* parent = nullptr) : QObject(parent) { }

    virtual ~SciQLopGraphComponentInterface() = default;

    inline virtual void set_pen(const QPen& pen) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_color(const QColor& color) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_line_style(GraphLineStyle style) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_marker_shape(GraphMarkerShape marker) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_marker_pen(const QPen& pen) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_line_width(const qreal width) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_visible(bool visible) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_name(const QString& name) noexcept { this->setObjectName(name); }

    inline virtual void set_selected(bool selected) noexcept
    {
        WARN_ABSTRACT_METHOD;
        Q_UNUSED(selected);
    }

    inline virtual QPen pen() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QPen();
    }

    inline virtual QColor color() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QColor();
    }

    inline virtual GraphLineStyle line_style() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return GraphLineStyle::NoLine;
    }

    inline virtual GraphMarkerShape marker_shape() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return GraphMarkerShape::NoMarker;
    }

    inline virtual QPen marker_pen() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QPen();
    }

    inline virtual qreal line_width() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return 0;
    }

    inline virtual bool visible() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    inline virtual QString name() const noexcept { return this->objectName(); }

    inline virtual bool selected() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void visible_changed(bool visible);
    Q_SIGNAL void name_changed(const QString& name);
    Q_SIGNAL void colors_changed(const QList<QColor>& colors);
    Q_SIGNAL void replot();
    Q_SIGNAL void selection_changed(bool selected);
};

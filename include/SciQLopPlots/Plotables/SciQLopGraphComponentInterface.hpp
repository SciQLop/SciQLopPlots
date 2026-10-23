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
#include "SciQLopPlots/Inspector/InspectorExtensionHolder.hpp"
#include "SciQLopPlots/SciQLopDrawnObject.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"

#include <QColor>
#include <QList>
#include <QObject>
#include <QPen>
#include <memory>

class SciQLopGraphComponentInterface : public SciQLopDrawnObject
{
    SciQLopPlotRange m_range;
    Q_OBJECT

protected:
    std::unique_ptr<InspectorExtensionHolder> m_extension_holder;

public:
    SciQLopGraphComponentInterface(QObject* parent = nullptr)
        : SciQLopDrawnObject(parent)
        , m_extension_holder { std::make_unique<InspectorExtensionHolder>(
              this, [this]() { Q_EMIT inspector_extensions_changed(); }) }
    {
    }

    virtual ~SciQLopGraphComponentInterface() = default;

    inline virtual void set_pen(const QPen& pen) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_line_style(GraphLineStyle style) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_marker_shape(GraphMarkerShape marker) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_marker_pen(const QPen& pen) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_marker_size(qreal size) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_line_width(const qreal width) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual QPen pen() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QPen();
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

    inline virtual qreal marker_size() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return 0;
    }

    inline virtual qreal line_width() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return 0;
    }

    virtual void set_fill_to_value(double value) { WARN_ABSTRACT_METHOD; }
    virtual void set_fill_between(SciQLopGraphComponentInterface* other) { WARN_ABSTRACT_METHOD; }
    virtual void clear_fill() { WARN_ABSTRACT_METHOD; }
    virtual void set_fill_color(const QColor& color) { WARN_ABSTRACT_METHOD; }
    virtual bool has_fill() const { return false; }
    virtual QColor fill_color() const { return {}; }
    virtual void set_error_bars(SciQLopGraphComponentInterface* lower,
                                SciQLopGraphComponentInterface* upper, double cap_width = 6.)
    {
        WARN_ABSTRACT_METHOD;
    }
    virtual void clear_error_bars() { WARN_ABSTRACT_METHOD; }
    virtual int component_index() const noexcept { return -1; }

    void add_inspector_extension(InspectorExtension* extension)
    {
        m_extension_holder->add(extension);
    }

    void remove_inspector_extension(InspectorExtension* extension)
    {
        m_extension_holder->remove(extension);
    }

    QList<InspectorExtension*> inspector_extensions() const
    {
        return m_extension_holder->list();
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void colors_changed(const QList<QColor>& colors);
    Q_SIGNAL void marker_pen_changed(const QPen& pen);
    Q_SIGNAL void marker_size_changed(qreal size);
    Q_SIGNAL void replot();
    Q_SIGNAL void fill_changed();
    Q_SIGNAL void error_bars_changed();
    Q_SIGNAL void inspector_extensions_changed();
};

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
#include <QFont>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <overlay.h>

class SciQLopOverlay : public QObject
{
    Q_OBJECT

    QPointer<QCPOverlay> _overlay;

public:
    explicit SciQLopOverlay(QCPOverlay* overlay, QObject* parent = nullptr)
        : QObject(parent), _overlay(overlay)
    {
        if (_overlay)
        {
            connect(_overlay, &QCPOverlay::messageChanged, this,
                    [this](const QString& text, QCPOverlay::Level level) {
                        Q_EMIT message_changed(text, static_cast<OverlayLevel>(level));
                    });
            connect(_overlay, &QCPOverlay::collapsedChanged, this,
                    &SciQLopOverlay::collapsed_changed);
        }
    }

    virtual ~SciQLopOverlay() override = default;

    inline QString text() const
    {
        return _overlay ? _overlay->text() : QString();
    }

    inline OverlayLevel level() const
    {
        return _overlay ? static_cast<OverlayLevel>(_overlay->level()) : OverlayLevel::Info;
    }

    inline OverlaySizeMode size_mode() const
    {
        return _overlay ? static_cast<OverlaySizeMode>(_overlay->sizeMode())
                        : OverlaySizeMode::Compact;
    }

    inline OverlayPosition position() const
    {
        return _overlay ? static_cast<OverlayPosition>(_overlay->position())
                        : OverlayPosition::Top;
    }

    inline bool is_collapsible() const { return _overlay ? _overlay->isCollapsible() : false; }

    inline bool is_collapsed() const { return _overlay ? _overlay->isCollapsed() : false; }

    inline qreal opacity() const { return _overlay ? _overlay->opacity() : 1.0; }

    inline QFont font() const { return _overlay ? _overlay->font() : QFont(); }

    inline QRect overlay_rect() const { return _overlay ? _overlay->overlayRect() : QRect(); }

    inline void show_message(const QString& text,
                             OverlayLevel level = OverlayLevel::Info,
                             OverlaySizeMode size_mode = OverlaySizeMode::Compact,
                             OverlayPosition position = OverlayPosition::Top)
    {
        if (_overlay)
            _overlay->showMessage(text, static_cast<QCPOverlay::Level>(level),
                                  static_cast<QCPOverlay::SizeMode>(size_mode),
                                  static_cast<QCPOverlay::Position>(position));
    }

    inline void clear_message()
    {
        if (_overlay)
            _overlay->clearMessage();
    }

    inline void set_collapsible(bool enabled)
    {
        if (_overlay)
            _overlay->setCollapsible(enabled);
    }

    inline void set_collapsed(bool collapsed)
    {
        if (_overlay)
            _overlay->setCollapsed(collapsed);
    }

    inline void set_opacity(qreal opacity)
    {
        if (_overlay)
            _overlay->setOpacity(opacity);
    }

    inline void set_font(const QFont& font)
    {
        if (_overlay)
            _overlay->setFont(font);
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void message_changed(const QString& text, OverlayLevel level);
    Q_SIGNAL void collapsed_changed(bool collapsed);
};

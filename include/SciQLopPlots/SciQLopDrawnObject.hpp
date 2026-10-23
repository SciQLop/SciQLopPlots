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
#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <optional>

/*!
 * \brief What everything drawn shares: visible, selected, name and colour.
 *
 * The public setters own the signals (docs/plans/2026-10-22-drawn-object-base-design.md): off
 * the owner thread they re-post themselves; on it they compare, apply through a _x hook, read
 * back and emit once. A family overrides the hooks it supports; the defaults throw
 * sqp::unsupported_call, NotImplementedError in Python.
 */
class SciQLopDrawnObject : public QObject
{
    Q_OBJECT

    std::optional<bool> m_last_visible;
    std::optional<bool> m_last_selected;
    std::optional<QColor> m_last_color;
    // Set while a setter applies: the parts' notifies wait for the setter's own, once all applied.
    bool m_applying_visible = false;
    bool m_applying_selected = false;
    bool m_applying_color = false;

public:
    explicit SciQLopDrawnObject(QObject* parent = nullptr);
    ~SciQLopDrawnObject() override = default;

    bool visible() const;
    void set_visible(bool visible);
    bool selected() const;
    void set_selected(bool selected);
    QString name() const;
    void set_name(const QString& name);
    QColor color() const;
    void set_color(const QColor& color);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void visible_changed(bool visible);
    Q_SIGNAL void selection_changed(bool selected);
    Q_SIGNAL void name_changed(const QString& name);
    Q_SIGNAL void color_changed(const QColor& color);

protected:
    // Not noexcept: the defaults throw, and so may an override (a throw out of noexcept terminates).
    virtual bool _visible() const;
    virtual void _set_visible(bool visible);
    virtual bool _selected() const;
    virtual void _set_selected(bool selected);
    virtual void _named(const QString& name) { Q_UNUSED(name); }
    virtual QColor _color() const;
    virtual void _set_color(const QColor& color);

    //! Aggregates (a graph over its components, a group over its members) apply even when they
    //! already read the value: showing a visible group still shows its one hidden member.
    virtual bool _always_apply() const { return false; }

    //! For changes NeoQCP makes itself (a click selects an item): emit unless it is a repeat.
    void _notify_visible(bool visible);
    void _notify_selected(bool selected);
    void _notify_color(const QColor& color);
    //! After a change made outside set_color (a new pen): emit if the colour moved from \a before.
    void _notify_color_since(const QColor& before);

    //! Record the current value without emitting. Skipped while a setter applies: it seeded the
    //! value it started from and emits against it (a part removed mid-loop must not swallow that).
    void _seed_visible(bool visible)
    {
        if (!m_applying_visible)
            m_last_visible = visible;
    }
    void _seed_selected(bool selected)
    {
        if (!m_applying_selected)
            m_last_selected = selected;
    }
    void _seed_color(const QColor& color)
    {
        if (!m_applying_color)
            m_last_color = color;
    }
};

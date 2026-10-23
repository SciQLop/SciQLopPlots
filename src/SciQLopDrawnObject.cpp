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
#include "SciQLopPlots/SciQLopDrawnObject.hpp"
#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"

#include <QDebug>
#include <QPointer>
#include <string>
#include <typeinfo>
#include <utility>

namespace
{
//! A queued set has no caller left to raise to, and a throw out of a queued slot terminates the
//! process: anything thrown there only warns.
// simplify: a worker-thread caller never learns its set was unsupported; upgrade path = a
// completion signal or a future returned by the setter, if a caller ever needs it.
template <typename Call>
void run_queued(const Call& call)
{
    try
    {
        call();
    }
    catch (const std::exception& e)
    {
        qWarning() << "SciQLopPlots:" << e.what();
    }
}

//! Off the owner thread: re-run \a call there and return true, the setter then returns.
template <typename Call>
bool hopped(SciQLopDrawnObject* self, Call call)
{
    return SciQLopPlots::posted_to_owner_thread(self, [call] { run_queued(call); });
}

template <typename T>
using Getter = T (SciQLopDrawnObject::*)() const;
template <typename Arg>
using Setter = void (SciQLopDrawnObject::*)(Arg);

//! What a family adds after "...not supported by <Class>": "; use colors()", or nothing.
std::string hint_of(const sqp::unsupported_call& e)
{
    const std::string what = e.what();
    const auto hint = what.find("; ");
    return hint == std::string::npos ? std::string() : what.substr(hint);
}

//! A setter whose getter is unsupported is unsupported too, and says so under its own name,
//! keeping the getter's hint.
template <typename T>
T current_value(SciQLopDrawnObject* self, Getter<T> get, const char* setter)
{
    try
    {
        return (self->*get)();
    }
    catch (const sqp::unsupported_call& e)
    {
        throw sqp::unsupported_call(unsupported_message(setter, typeid(*self).name()) + hint_of(e));
    }
}

template <typename T, typename Arg>
void apply_change(SciQLopDrawnObject* self, const char* setter, bool always_apply, const T& value,
                  Getter<T> get, Setter<Arg> set, Setter<Arg> seed, Setter<Arg> notify)
{
    const T before = current_value(self, get, setter);
    if (before == value && !always_apply)
        return;
    // Seeded with the live value, before _set_x and never after: a change that bypassed the
    // setters must not make this one's signal look like a repeat.
    (self->*seed)(before);
    // A slot run while applying may delete self (a member's slot removing its group).
    QPointer<SciQLopDrawnObject> alive = self;
    (self->*set)(value);
    if (!alive)
        return;
    if (const T after = (self->*get)(); after != before)
        (self->*notify)(after);
}
}

SciQLopDrawnObject::SciQLopDrawnObject(QObject* parent) : QObject(parent) { }

bool SciQLopDrawnObject::visible() const
{
    return _visible();
}

void SciQLopDrawnObject::set_visible(bool visible)
{
    if (hopped(this, [this, visible] { set_visible(visible); }))
        return;
    apply_change(this, "set_visible", _always_apply(), visible, &SciQLopDrawnObject::_visible,
                 &SciQLopDrawnObject::_set_visible, &SciQLopDrawnObject::_seed_visible,
                 &SciQLopDrawnObject::_notify_visible);
}

bool SciQLopDrawnObject::selected() const
{
    return _selected();
}

void SciQLopDrawnObject::set_selected(bool selected)
{
    if (hopped(this, [this, selected] { set_selected(selected); }))
        return;
    apply_change(this, "set_selected", _always_apply(), selected, &SciQLopDrawnObject::_selected,
                 &SciQLopDrawnObject::_set_selected, &SciQLopDrawnObject::_seed_selected,
                 &SciQLopDrawnObject::_notify_selected);
}

QString SciQLopDrawnObject::name() const
{
    return objectName();
}

void SciQLopDrawnObject::set_name(const QString& name)
{
    if (hopped(this, [this, name] { set_name(name); }))
        return;
    if (objectName() == name)
        return;
    setObjectName(name);
    _named(name);
    Q_EMIT name_changed(name);
}

QColor SciQLopDrawnObject::color() const
{
    return _color();
}

void SciQLopDrawnObject::set_color(const QColor& color)
{
    if (hopped(this, [this, color] { set_color(color); }))
        return;
    apply_change(this, "set_color", _always_apply(), color, &SciQLopDrawnObject::_color,
                 &SciQLopDrawnObject::_set_color, &SciQLopDrawnObject::_seed_color,
                 &SciQLopDrawnObject::_notify_color);
}

void SciQLopDrawnObject::_notify_visible(bool visible)
{
    if (std::exchange(m_last_visible, visible) != visible)
        Q_EMIT visible_changed(visible);
}

void SciQLopDrawnObject::_notify_selected(bool selected)
{
    if (std::exchange(m_last_selected, selected) != selected)
        Q_EMIT selection_changed(selected);
}

void SciQLopDrawnObject::_notify_color(const QColor& color)
{
    if (std::exchange(m_last_color, color) != color)
        Q_EMIT color_changed(color);
}

bool SciQLopDrawnObject::_visible() const
{
    throw_unsupported("visible", *this);
}

void SciQLopDrawnObject::_set_visible(bool)
{
    throw_unsupported("set_visible", *this);
}

bool SciQLopDrawnObject::_selected() const
{
    throw_unsupported("selected", *this);
}

void SciQLopDrawnObject::_set_selected(bool)
{
    throw_unsupported("set_selected", *this);
}

QColor SciQLopDrawnObject::_color() const
{
    throw_unsupported("color", *this);
}

void SciQLopDrawnObject::_set_color(const QColor&)
{
    throw_unsupported("set_color", *this);
}

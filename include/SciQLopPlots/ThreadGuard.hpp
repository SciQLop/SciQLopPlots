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

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThread>

#include <stdexcept>
#include <string>
#include <utility>

namespace SciQLopPlots
{

/*!
 * Called off \a owner's thread (a Python thread, SciQLop#147), posts \a call to it and returns
 * true: the mutator returns, and \a call runs it again on the owner's thread.
 *
 * Queued, never blocking: the GUI thread may be waiting on the caller (the GIL), so a blocking
 * hop can deadlock. \a owner is the call's context object: Qt drops the call if it is deleted
 * first, so the call needs no QPointer of its own.
 */
template <typename Call>
[[nodiscard]] bool posted_to_owner_thread(QObject* owner, Call&& call)
{
    if (QThread::currentThread() == owner->thread())
        return false;
    QMetaObject::invokeMethod(owner, std::forward<Call>(call), Qt::QueuedConnection);
    return true;
}

/*!
 * For calls that hand an object back (constructors, create_*): they cannot be queued, and handing
 * the object across threads would need a blocking hop. Off the owner's thread they throw, which
 * the bindings raise as a Python RuntimeError.
 */
inline void require_owner_thread(const QObject* owner, const char* what)
{
    if (owner && QThread::currentThread() != owner->thread())
        throw std::runtime_error(std::string(what)
                                 + " must be called from the thread the plot lives in (the GUI "
                                   "thread); call it there, e.g. via a queued invocation");
}

//! \a owner, after require_owner_thread: for a constructor's first base-class initializer, so
//! nothing is built off-thread.
template <typename Owner>
Owner* on_owner_thread(Owner* owner, const char* what)
{
    require_owner_thread(owner, what);
    return owner;
}

} // namespace SciQLopPlots

//! First statement of a void mutator of a QObject: off the object's thread, re-runs \a call
//! (the mutator calling itself with its own arguments, copied) on that thread and returns.
#define SCIQLOP_ON_OWNER_THREAD(call)                                                              \
    if (::SciQLopPlots::posted_to_owner_thread(this, [=, this] { call; }))                         \
    return

//! SCIQLOP_ON_OWNER_THREAD for a mutator taking the QObject* \a arg: the queued call holds it in
//! a QPointer and is skipped if \a arg is deleted before it runs.
#define SCIQLOP_ON_OWNER_THREAD_WITH(arg, call)                                                    \
    if (::SciQLopPlots::posted_to_owner_thread(                                                    \
            this, [=, this, guarded = QPointer(arg)] {                                             \
                if (auto* arg = guarded.data())                                                    \
                    call;                                                                          \
            }))                                                                                    \
    return

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
#include <QHash>
#include <QMutex>
#include <QString>
#include <QVariant>
#include <atomic>
#include <memory>

// Holds an optional, externally-supplied per-leaf score overlay (e.g. from a
// Python-side smart-search method) blended into the built-in lexical score.
// Every method is safe to call from any thread: writers build a whole new
// immutable QHash off any lock, then only hold m_mutex long enough to swap a
// shared_ptr; readers copy the shared_ptr under the same short lock and read
// the (now locally-owned) snapshot lock-free. This mirrors the shared_ptr
// snapshot pattern used to fix a prior GIL/mutex deadlock (PR #70) -- the
// lock is never held across a call that could block on the GIL.
class ExternalScoreOverlay
{
public:
    void set_scores(const QHash<QString, QVariant>& scores);
    void set_enabled(bool enabled) noexcept;
    bool enabled() const noexcept;
    double score_for(const QString& path_key) const;

private:
    mutable QMutex m_mutex;
    std::shared_ptr<const QHash<QString, double>> m_scores;
    std::atomic<bool> m_enabled { false };
};

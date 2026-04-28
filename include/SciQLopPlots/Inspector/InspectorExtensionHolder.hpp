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
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once
#include "InspectorExtension.hpp"
#include <QList>
#include <QObject>
#include <QPointer>
#include <functional>

class InspectorExtensionHolder
{
    struct Entry
    {
        QPointer<InspectorExtension> ext;
        QMetaObject::Connection conn;
    };

    QList<Entry> m_entries;
    QObject* m_owner;
    std::function<void()> m_notify;

public:
    InspectorExtensionHolder(QObject* owner, std::function<void()> notify)
        : m_owner(owner), m_notify(std::move(notify))
    {
    }

    bool add(InspectorExtension* extension)
    {
        if (!extension)
            return false;
        for (const auto& e : m_entries)
            if (e.ext.data() == extension)
                return false;
        extension->setParent(m_owner);
        auto conn = QObject::connect(
            extension, &QObject::destroyed, m_owner, [this]() { m_notify(); });
        m_entries.append({ QPointer<InspectorExtension>(extension), conn });
        m_notify();
        return true;
    }

    bool remove(InspectorExtension* extension)
    {
        bool removed = false;
        for (int i = m_entries.size() - 1; i >= 0; --i)
        {
            auto& e = m_entries[i];
            if (!e.ext || e.ext.data() == extension)
            {
                QObject::disconnect(e.conn);
                m_entries.removeAt(i);
                removed = true;
            }
        }
        if (removed)
        {
            if (extension && extension->parent() == m_owner)
                extension->setParent(nullptr);
            m_notify();
        }
        return removed;
    }

    QList<InspectorExtension*> list() const
    {
        QList<InspectorExtension*> out;
        out.reserve(m_entries.size());
        for (const auto& e : m_entries)
            if (e.ext)
                out.append(e.ext.data());
        return out;
    }
};

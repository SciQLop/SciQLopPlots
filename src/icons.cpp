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
#include "SciQLopPlots/Icons/icons.hpp"
#include <QDir>
#include <QFile>
#include <qapplicationstatic.h>

Icons::Icons(QObject* parent) : QObject(parent)
{
    m_icons[""] = QIcon();
    for (const auto& entry : QDir(":/icons/theme").entryInfoList(QDir::Files))
    {
        m_icons[entry.fileName()] = QIcon(entry.filePath());
        m_icons[entry.baseName()] = QIcon(entry.filePath());
        m_icons[entry.filePath()] = QIcon(entry.filePath());
    }
}

const QIcon& Icons::get_icon(const QString& name)
{
    if (!instance()->m_icons.contains(name))
    {
        if (!QFile::exists(name))
            return instance()->m_icons[""];
        instance()->m_icons[name] = QIcon(name);
    }
    return instance()->m_icons[name];
}

void Icons::add_icon(const QString& name, const QIcon& icon, const QStringList& aliases)
{
    instance()->m_icons[name] = icon;
    for (const auto& alias : aliases)
    {
        instance()->m_icons[alias] = icon;
    }
}

Q_APPLICATION_STATIC(Icons, _icons);

Icons* Icons::instance()
{
    return _icons();
}

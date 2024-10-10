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
#include <QMap>
#include <QSet>
#include <QString>

class UniqueNamesFactory
{
    UniqueNamesFactory() { }

    QMap<QString, QSet<QString>> _used_names;

    inline QString _unique_name(const QString& prefix)
    {
        if (!_used_names.contains(prefix))
        {
            _used_names.insert(prefix, QSet<QString>());
        }
        QString name = prefix;
        int i = 0;
        while (_used_names[prefix].contains(name))
        {
            name = prefix + QString::number(i++);
        }
        _used_names[prefix].insert(name);
        return name;
    }

public:
    inline static UniqueNamesFactory& instance()
    {
        static UniqueNamesFactory instance;
        return instance;
    }

    inline static QString unique_name(const QString& prefix)
    {
        return instance()._unique_name(prefix);
    }
};

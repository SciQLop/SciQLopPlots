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
#include <QIcon>
#include <QMap>
#include <QObject>

class Icons : public QObject
{
    Q_OBJECT

    QMap<QString, QIcon> m_icons;


public:
    Icons(QObject* parent = nullptr);
    virtual ~Icons() = default;

    static const QIcon& get_icon(const QString& name);
    static void add_icon(const QString& name, const QIcon& icon, const QStringList& aliases = {});

    inline static QStringList icons() { return instance()->m_icons.keys(); }

    static Icons* instance();
};

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
#include <QMap>
#include <QObject>

class InspectorBase;

class Inspectors : public QObject
{

    Q_OBJECT

    QMap<QString, InspectorBase*> m_inspectors;

    Inspectors() { }

public:
    virtual ~Inspectors() = default;

    inline static Inspectors& instance()
    {
        static Inspectors instance;
        return instance;
    }

    inline static int registerInspector(const QString& typeName, InspectorBase* inspector)
    {
        instance().m_inspectors[typeName] = inspector;
        return instance().m_inspectors.size();
    }

    template <typename T>
    inline static auto
    registerInspector(InspectorBase* inspector) -> decltype(T::staticMetaObject.className(), int())
    {
        return registerInspector(T::staticMetaObject.className(), inspector);
    }

    inline static InspectorBase* inspector(const QString& typeName)
    {
        return instance().m_inspectors.value(typeName, nullptr);
    }

    static InspectorBase* inspector(const QObject* obj);
};

#define REGISTER_INSPECTOR(INSPECTOR)                                                              \
    static int _id_##INSPECTOR                                                                     \
        = Inspectors::registerInspector<typename INSPECTOR::compatible_type>(new INSPECTOR());

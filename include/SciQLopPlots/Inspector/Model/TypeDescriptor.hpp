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
#include <QList>
#include <QMap>
#include <QMetaObject>
#include <QObject>
#include <functional>

struct TypeDescriptor
{
    std::function<QList<QObject*>(QObject*)> children;

    std::function<QList<QMetaObject::Connection>(QObject* obj,
        std::function<void(QObject*)> add_child,
        std::function<void(QObject*)> remove_child)>
        connect_children;

    std::function<QIcon(const QObject*)> icon = {};
    std::function<QString(const QObject*)> tooltip = {};
    std::function<void(QObject*, bool)> set_selected = {};

    bool deletable = true;
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
};

class TypeRegistry
{
    QMap<QString, TypeDescriptor> m_descriptors;
    TypeRegistry() = default;

public:
    static TypeRegistry& instance();

    template <typename T>
    void register_type(TypeDescriptor desc)
    {
        m_descriptors[T::staticMetaObject.className()] = std::move(desc);
    }

    void register_type(const QString& type_name, TypeDescriptor desc);

    const TypeDescriptor* descriptor(const QObject* obj) const;
};

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
#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QFormLayout>
#include <QWidget>
#include <fmt/format.h>

class PropertyDelegateBase : public QWidget
{
    Q_OBJECT

protected:
    QFormLayout* m_layout;
    QLabel* m_title;

protected:
    QPointer<QObject> m_object = nullptr;

    template <typename T>
    static T* as_type(QObject* object)
    {
        return qobject_cast<T*>(object);
    }

public:
    PropertyDelegateBase(QObject* object, QWidget* parent = nullptr);

    Q_SLOT void object_name_changed(const QString& name)
    {
        m_title->setText(fmt::format("{} properties", name.toStdString()).c_str());
    }

    virtual ~PropertyDelegateBase() = default;
};

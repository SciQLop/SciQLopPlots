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
#include <QObject>

class PropertyDelegateBase;

class PropertyDelegateFactoryInterface
{
public:
    virtual ~PropertyDelegateFactoryInterface() = default;
    virtual PropertyDelegateBase* create(QObject* object, QWidget* parent) = 0;
};

template <typename T>
class PropertyDelegateFactory : public PropertyDelegateFactoryInterface
{
    PropertyDelegateBase* create(QObject* object, QWidget* parent) override
    {
        return new T(qobject_cast<typename T::compatible_type*>(object), parent);
    }
};

class PropertyDelegates : public QObject
{

    Q_OBJECT

    QMap<QString, PropertyDelegateFactoryInterface*> m_delegates_factories;

    PropertyDelegates() { }

    inline static int registerDelegate(
        const QString& typeName, PropertyDelegateFactoryInterface* delegate)
    {
        instance().m_delegates_factories[typeName] = delegate;
        return instance().m_delegates_factories.size();
    }

public:
    virtual ~PropertyDelegates() = default;

    inline static PropertyDelegates& instance()
    {
        static PropertyDelegates instance;
        return instance;
    }

    template <typename DelegateType>
    inline static auto registerDelegate()
        -> decltype(DelegateType::compatible_type::staticMetaObject.className(), int())
    {
        return registerDelegate(DelegateType::compatible_type::staticMetaObject.className(),
            new PropertyDelegateFactory<DelegateType>());
    }

    inline static PropertyDelegateBase* delegate(
        const QString& typeName, QObject* object, QWidget* parent = nullptr)
    {
        auto factory = instance().m_delegates_factories.value(typeName, nullptr);
        return factory ? factory->create(object, parent) : nullptr;
    }

    inline static PropertyDelegateBase* delegate(QObject* obj, QWidget* parent = nullptr)
    {
        auto metaObject = obj->metaObject();
        PropertyDelegateBase* _delegate = nullptr;
        do
        {
            _delegate = delegate(metaObject->className(), obj, parent);
            metaObject = metaObject->superClass();
        } while (_delegate == nullptr && metaObject != nullptr);
        return _delegate;
    }
};

#define REGISTER_DELEGATE(DELEGATE)                                                                \
    static int _id_##DELEGATE = PropertyDelegates::registerDelegate<DELEGATE>();

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
#include "SciQLopPlots/Inspector/InspectorBase.hpp"
#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include <QIcon>
#include <QList>
#include <QObject>

class SciQLopGraphComponentInspector : public InspectorBase
{
    Q_OBJECT

    inline SciQLopGraphComponentInterface* _component(QObject* obj)
    {
        return qobject_cast<SciQLopGraphComponentInterface*>(obj);
    }

    inline const SciQLopGraphComponentInterface* _component(const QObject* obj)
    {
        return qobject_cast<const SciQLopGraphComponentInterface*>(obj);
    }

public:
    using compatible_type = SciQLopGraphComponentInterface;

    SciQLopGraphComponentInspector() : InspectorBase() { }

    virtual QList<QObject*> children(QObject* obj) Q_DECL_OVERRIDE;

    virtual QObject* child(const QString& name, QObject* obj) Q_DECL_OVERRIDE;

    inline virtual QIcon icon(const QObject* const obj) Q_DECL_OVERRIDE
    {
        Q_UNUSED(obj);
        return QIcon();
    }

    inline virtual QString tooltip(const QObject* const obj) Q_DECL_OVERRIDE
    {
        Q_UNUSED(obj);
        return QString();
    }

    virtual void connect_node(PlotsModelNode* node, QObject* const obj) Q_DECL_OVERRIDE;

    virtual void set_selected(QObject* obj, bool selected) Q_DECL_OVERRIDE;

    virtual bool selected(const QObject* obj) Q_DECL_OVERRIDE;

    inline virtual bool deletable(const QObject* obj) Q_DECL_OVERRIDE { return false; }
};

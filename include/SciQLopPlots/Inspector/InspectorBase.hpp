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
#include <QObject>

class PlotsModelNode;

class InspectorBase : public QObject
{
    Q_OBJECT

public:
    using compatible_type = QObject;
    InspectorBase(QObject* parent = nullptr);
    virtual ~InspectorBase() = default;

    virtual QList<QObject*> children(QObject* obj);
    virtual QObject* child(const QString& name, QObject* obj);
    virtual QIcon icon(const QObject* const obj);
    virtual QString tooltip(const QObject* const obj);
    virtual void connect_node(PlotsModelNode* node, QObject* const obj);
    virtual void set_selected(QObject* obj, bool selected);
    virtual bool selected(const QObject* obj);
    virtual bool deletable(const QObject* obj);
    virtual Qt::ItemFlags flags();
};

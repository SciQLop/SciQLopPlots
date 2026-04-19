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
#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"
#include "SciQLopPlots/Inspector/InspectorExtension.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include <QGroupBox>
#include <QVBoxLayout>
#include <algorithm>

static constexpr auto EXT_GROUP_PREFIX = "__inspector_ext__";

PropertyDelegateBase::PropertyDelegateBase(QObject *object, QWidget *parent)
        : QWidget(parent), m_object { object }
{
    m_layout = new QFormLayout();
    setLayout(m_layout);
    m_title = new QLabel();
    m_layout->addWidget(m_title);
    connect(object, &QObject::objectNameChanged, this,
            &PropertyDelegateBase::object_name_changed);
    object_name_changed(object->objectName());
}

void PropertyDelegateBase::append_inspector_extensions()
{
    auto* plottable = qobject_cast<SciQLopPlottableInterface*>(m_object.data());
    if (!plottable)
        return;
    rebuild_inspector_extensions();
    connect(plottable, &SciQLopPlottableInterface::inspector_extensions_changed, this,
            &PropertyDelegateBase::rebuild_inspector_extensions, Qt::UniqueConnection);
}

void PropertyDelegateBase::rebuild_inspector_extensions()
{
    auto* plottable = qobject_cast<SciQLopPlottableInterface*>(m_object.data());
    if (!plottable)
        return;

    const auto prior = findChildren<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* gb : prior)
    {
        if (gb->objectName().startsWith(EXT_GROUP_PREFIX))
        {
            m_layout->removeWidget(gb);
            gb->deleteLater();
        }
    }

    auto exts = plottable->inspector_extensions();
    std::sort(exts.begin(), exts.end(),
              [](InspectorExtension* a, InspectorExtension* b)
              { return a->priority() < b->priority(); });

    for (auto* ext : exts)
    {
        auto* group = new QGroupBox(ext->section_title(), this);
        group->setObjectName(QString(EXT_GROUP_PREFIX) + QString::number(quintptr(ext)));
        auto* outer = new QVBoxLayout(group);
        outer->setContentsMargins(6, 6, 6, 6);
        if (auto* w = ext->build_widget(group))
            outer->addWidget(w);
        m_layout->addRow(group);
        connect(ext, &InspectorExtension::invalidated, this,
                &PropertyDelegateBase::rebuild_inspector_extensions, Qt::UniqueConnection);
    }
}

/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Inspector/PropertiesDelegates/InspectorExtensionDelegate.hpp"
#include "SciQLopPlots/Inspector/InspectorExtension.hpp"
#include <QGroupBox>
#include <QVBoxLayout>

InspectorExtension* InspectorExtensionDelegate::extension() const
{
    return as_type<InspectorExtension>(m_object);
}

static QGroupBox* _build_group(InspectorExtension* ext, QWidget* parent)
{
    auto* group = new QGroupBox(ext->section_title(), parent);
    auto* outer = new QVBoxLayout(group);
    outer->setContentsMargins(6, 6, 6, 6);
    if (auto* w = ext->build_widget(group))
        outer->addWidget(w);
    return group;
}

InspectorExtensionDelegate::InspectorExtensionDelegate(InspectorExtension* object,
                                                       QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    m_layout->addRow(_build_group(object, this));

    connect(object, &InspectorExtension::invalidated, this,
            [this, object]()
            {
                const auto groups = findChildren<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly);
                for (auto* gb : groups)
                {
                    m_layout->removeWidget(gb);
                    gb->deleteLater();
                }
                m_layout->addRow(_build_group(object, this));
            });
}

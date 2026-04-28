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
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include <QGroupBox>
#include <QVBoxLayout>
#include <algorithm>

static constexpr auto EXT_GROUP_PREFIX = "__inspector_ext__";

template <typename T>
static bool _try_collect(QObject* obj, QList<InspectorExtension*>& out)
{
    if (auto* typed = qobject_cast<T*>(obj))
    {
        out = typed->inspector_extensions();
        return true;
    }
    return false;
}

static QList<InspectorExtension*> _collect_extensions(QObject* obj)
{
    QList<InspectorExtension*> out;
    _try_collect<SciQLopPlottableInterface>(obj, out)
        || _try_collect<SciQLopMultiPlotPanel>(obj, out)
        || _try_collect<SciQLopPlotInterface>(obj, out)
        || _try_collect<SciQLopPlotAxisInterface>(obj, out)
        || _try_collect<SciQLopGraphComponentInterface>(obj, out);
    return out;
}

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
    auto* obj = m_object.data();
    if (!obj)
        return;

    auto try_connect = [&](auto* typed) -> bool
    {
        if (!typed)
            return false;
        connect(typed, &std::remove_pointer_t<decltype(typed)>::inspector_extensions_changed, this,
                &PropertyDelegateBase::rebuild_inspector_extensions, Qt::UniqueConnection);
        return true;
    };

    bool connected = try_connect(qobject_cast<SciQLopPlottableInterface*>(obj))
        || try_connect(qobject_cast<SciQLopMultiPlotPanel*>(obj))
        || try_connect(qobject_cast<SciQLopPlotInterface*>(obj))
        || try_connect(qobject_cast<SciQLopPlotAxisInterface*>(obj))
        || try_connect(qobject_cast<SciQLopGraphComponentInterface*>(obj));
    if (connected)
        rebuild_inspector_extensions();
}

void PropertyDelegateBase::rebuild_inspector_extensions()
{
    const auto prior = findChildren<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* gb : prior)
    {
        if (gb->objectName().startsWith(EXT_GROUP_PREFIX))
        {
            m_layout->removeWidget(gb);
            gb->deleteLater();
        }
    }

    auto exts = _collect_extensions(m_object.data());
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

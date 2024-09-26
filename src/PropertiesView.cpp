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
#include "SciQLopPlots/Inspector/View/PropertiesView.hpp"
#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"
#include "SciQLopPlots/Inspector/PropertyDelegates.hpp"
#include <QVBoxLayout>

void PropertiesView::set_current_objects(const QList<QObject*>& objects)
{
    if (m_delegateWidget)
    {
        delete m_delegateWidget;
        m_delegateWidget = nullptr;
    }
    if (!objects.isEmpty())
    {
        auto first = objects.first();
        auto delegate = PropertyDelegates::delegate(first, this);
        if (delegate)
        {
            m_delegateWidget = delegate;
            this->layout()->addWidget(m_delegateWidget);
        }
    }
}

PropertiesView::PropertiesView(QWidget* parent)
{
    this->setLayout(new QVBoxLayout(this));
}

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
#include "SciQLopPlots/Inspector/View/PropertiesPanel.hpp"
#include "SciQLopPlots/Inspector/View/InspectorView.hpp"
#include "SciQLopPlots/Inspector/View/PropertiesView.hpp"
#include <QSplitter>
#include <QVBoxLayout>

PropertiesPanel::PropertiesPanel(QWidget* parent)
{
    this->setWindowTitle("Properties");
    auto splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    setLayout(new QVBoxLayout(this));
    layout()->addWidget(splitter);
    m_inspectorView = new InspectorView(this);
    m_propertiesView = new PropertiesView(this);
    splitter->addWidget(m_inspectorView);
    splitter->addWidget(m_propertiesView);
    connect(m_inspectorView, &InspectorView::objects_selected, m_propertiesView,
            &PropertiesView::set_current_objects);
}

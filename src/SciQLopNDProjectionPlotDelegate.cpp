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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopNDProjectionPlotDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"

#include <QComboBox>
#include <QSignalBlocker>

namespace
{
const QList<QPair<ProjectionAxisLinking, QString>> kAxisLinkings {
    { ProjectionAxisLinking::Independent, "Independent" },
    { ProjectionAxisLinking::ByDimension, "Linked by dimension" },
    { ProjectionAxisLinking::SameScale, "Same scale on all axes" },
};
}

SciQLopNDProjectionPlotDelegate::SciQLopNDProjectionPlotDelegate(SciQLopNDProjectionPlot* object,
                                                                 QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    add_axis_linking_row(object);
    add_square_panes_row(object);
    // simplify: these three are set from the plot once, when the panel opens; they have no
    // change signal yet. Add one when code starts changing them while the panel is open.
    add_switch("Shared legend", object->shared_legend(), &SciQLopNDProjectionPlot::set_shared_legend);
    add_switch("Linked crosshairs", object->linked_crosshairs(),
               &SciQLopNDProjectionPlot::set_linked_crosshairs);
    add_switch("Colour along time", object->time_color_enabled(),
               &SciQLopNDProjectionPlot::set_time_color_enabled);
    append_inspector_extensions();
}

void SciQLopNDProjectionPlotDelegate::add_axis_linking_row(SciQLopNDProjectionPlot* plot)
{
    auto* combo = new QComboBox;
    combo->setObjectName("axis_linking");
    for (const auto& [linking, name] : kAxisLinkings)
        combo->addItem(name, QVariant::fromValue(linking));
    const auto show = [combo](ProjectionAxisLinking linking)
    {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(combo->findData(QVariant::fromValue(linking)));
    };
    show(plot->axis_linking());
    fit_combo_to_content(combo);
    m_layout->addRow("Axes", combo);
    connect(combo, &QComboBox::currentIndexChanged, this,
            [this, combo](int)
            {
                if (auto* p = projection())
                    p->set_axis_linking(combo->currentData().value<ProjectionAxisLinking>());
            });
    connect(plot, &SciQLopNDProjectionPlot::axis_linking_changed, combo, show);
}

void SciQLopNDProjectionPlotDelegate::add_square_panes_row(SciQLopNDProjectionPlot* plot)
{
    auto* square = new BooleanDelegate("Square panes", plot->equal_aspect_ratio());
    square->setObjectName("square_panes");
    m_layout->addRow("", square);
    const auto lock = [square](ProjectionAxisLinking linking)
    { square->setEnabled(linking != ProjectionAxisLinking::SameScale); };
    lock(plot->axis_linking());
    connect(square, &BooleanDelegate::value_changed, this,
            [this](bool on)
            {
                if (auto* p = projection())
                    p->set_equal_aspect_ratio(on);
            });
    connect(plot, &SciQLopPlotInterface::equal_aspect_ratio_changed, square,
            &BooleanDelegate::set_value);
    connect(plot, &SciQLopNDProjectionPlot::axis_linking_changed, square, lock);
}

void SciQLopNDProjectionPlotDelegate::add_switch(const QString& text, bool value,
                                                 void (SciQLopNDProjectionPlot::*setter)(bool))
{
    auto* box = new BooleanDelegate(text, value);
    m_layout->addRow("", box);
    connect(box, &BooleanDelegate::value_changed, this,
            [this, setter](bool on)
            {
                if (auto* p = projection())
                    (p->*setter)(on);
            });
}

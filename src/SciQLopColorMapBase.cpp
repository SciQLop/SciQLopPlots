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
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"

SciQLopColorMapBase::SciQLopColorMapBase(SciQLopPlotAxis* keyAxis, SciQLopPlotAxis* valueAxis,
                                         SciQLopPlotColorScaleAxis* colorScaleAxis,
                                         QVariantMap metaData, QObject* parent)
    : SciQLopColorMapInterface(std::move(metaData), parent)
    , _keyAxis{keyAxis}
    , _valueAxis{valueAxis}
    , _colorScaleAxis{colorScaleAxis}
{
}

// Derived destructors handle plottable removal before their QPointer is cleared.
// The base destructor must NOT call plottable() (pure virtual in base context).
SciQLopColorMapBase::~SciQLopColorMapBase() = default;

void SciQLopColorMapBase::set_selected(bool selected) noexcept
{
    if (_selected != selected)
    {
        _selected = selected;
        auto legend_item = _legend_item();
        if (legend_item && legend_item->selected() != selected)
            legend_item->setSelected(selected);
        emit selection_changed(selected);
    }
}

bool SciQLopColorMapBase::selected() const noexcept
{
    return _selected;
}

void SciQLopColorMapBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _keyAxis = qcp_axis;
        if (auto* p = plottable(); p)
            p->setKeyAxis(qcp_axis->qcp_axis());
    }
}

void SciQLopColorMapBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _valueAxis = qcp_axis;
        if (auto* p = plottable(); p)
            p->setValueAxis(qcp_axis->qcp_axis());
    }
}

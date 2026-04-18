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

#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QScrollArea;
class QVBoxLayout;

class SciQLopWaterfallDelegate : public PropertyDelegateBase
{
    Q_OBJECT

    SciQLopWaterfallGraph* waterfall() const { return as_type<SciQLopWaterfallGraph>(m_object); }

    QComboBox* m_modeCombo = nullptr;
    QDoubleSpinBox* m_spacingSpin = nullptr;
    QScrollArea* m_offsetsScroll = nullptr;
    QVBoxLayout* m_offsetsLayout = nullptr;
    QList<QDoubleSpinBox*> m_offsetsSpins;
    QCheckBox* m_normalizeCheck = nullptr;
    QDoubleSpinBox* m_gainSpin = nullptr;

    void rebuild_offsets_editor();
    void refresh_mode_enabled_state();

public:
    using compatible_type = SciQLopWaterfallGraph;
    SciQLopWaterfallDelegate(SciQLopWaterfallGraph* object, QWidget* parent = nullptr);
    virtual ~SciQLopWaterfallDelegate() = default;
};

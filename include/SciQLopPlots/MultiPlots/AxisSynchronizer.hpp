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

#include "SciQLopPlotCollection.hpp"

class SciQLopPlotPanelInterface;

class AxisSynchronizer : public SciQLopPlotCollectionBehavior
{
    Q_OBJECT

protected:
    SciQLopPlotRange _last_range;
    AxisType m_sync_axis = AxisType::XAxis;


public:
    AxisSynchronizer(AxisType axis, QObject* parent = nullptr)
            : SciQLopPlotCollectionBehavior(parent), m_sync_axis { axis }
    {
    }

    Q_SLOT virtual void updatePlotList(const QList<QPointer<SciQLopPlotInterface>>& plots) override;
    Q_SLOT virtual void plotAdded(SciQLopPlotInterface* plot) override;
    Q_SLOT virtual void panelAdded(SciQLopPlotPanelInterface* panel) override;
    Q_SLOT virtual void panelRemoved(SciQLopPlotPanelInterface* panel) override;
    Q_SLOT virtual void set_axis_range(const SciQLopPlotRange& range);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(const SciQLopPlotRange& range);
};

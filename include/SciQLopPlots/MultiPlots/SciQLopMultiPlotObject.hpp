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
#include <QObject>
class SciQLopPlot;

class SciQLopMultiPlotObject : public QObject
{
    Q_OBJECT

protected:
    QList<QPointer<SciQLopPlotInterface>> m_plots;

    virtual void addObject(SciQLopPlotInterface* plot);
    virtual void removeObject(SciQLopPlotInterface* plot);
    void replotAll();

public:
    explicit SciQLopMultiPlotObject(SciQLopPlotCollectionInterface* parent = nullptr);

    virtual ~SciQLopMultiPlotObject() Q_DECL_OVERRIDE;

    Q_SLOT virtual void updatePlotList(const QList<QPointer<SciQLopPlotInterface>>& plots);
};

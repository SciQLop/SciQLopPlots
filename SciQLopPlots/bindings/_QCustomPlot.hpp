/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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

#include <SciQLopPlots/SciQLopGraph.hpp>
#include <qcustomplot.h>

class _QCustomPlot : public QCustomPlot
{
    Q_OBJECT
public:
    explicit _QCustomPlot(QWidget* parent = nullptr) : QCustomPlot { parent } {};
    virtual ~_QCustomPlot() Q_DECL_OVERRIDE {};
    inline QCPColorMap* addColorMap(QCPAxis* x, QCPAxis* y)
    {
        auto cm = new QCPColorMap(x, y);
        return cm;
    }

    inline SciQLopGraph* addSciQLopGraph(QCPAxis* x, QCPAxis* y, QStringList labels,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        auto sg = new SciQLopGraph(this, x, y, labels, dataOrder);
        return sg;
    }
};

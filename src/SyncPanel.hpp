/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2020, Plasma Physics Laboratory - CNRS
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

#include <QObject>
#include <QWidget>

#include <QVBoxLayout>

#include <list>
#include <cassert>

#include "IPlotWidget.hpp"

namespace SciQLopPlots
{
class SyncPannel : public QWidget
{
    Q_OBJECT
    std::list<IPlotWidget*> plots;
    AxisRange currentRange;

public:
    explicit SyncPannel(QWidget* parent = nullptr) : QWidget(parent),currentRange(0.,0.)
    {
        setLayout(new QVBoxLayout);
    }

    ~SyncPannel(){}

    inline void setXRange(AxisRange newRange) noexcept
    {
        if(currentRange!=newRange)
        {
            currentRange = newRange;
            for(auto plot:plots)
                plot->setXRange(newRange);
        }
    }

    inline void addPlot(IPlotWidget* plot)
    {
        assert(plot);
        plots.push_back(plot);
        this->layout()->addWidget(plot);
        plot->setXRange(currentRange);
        connect(plot,&IPlotWidget::xRangeChanged, this,&SyncPannel::setXRange);
    }
};
}

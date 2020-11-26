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
#include <QScrollArea>
#include <QWidget>
#include <QTimer>

#include <QVBoxLayout>

#include <cassert>
#include <list>

#include "IPlotWidget.hpp"

namespace SciQLopPlots
{
class SyncPannel : public QScrollArea
{
    Q_OBJECT
    std::list<IPlotWidget*> plots;
    AxisRange currentRange;
    QTimer* refreshTimer;
public:
    explicit SyncPannel(QWidget* parent = nullptr) : QScrollArea(parent), currentRange(0., 0.)
    {
        refreshTimer = new QTimer{this};
        refreshTimer->setSingleShot(true);
        connect(this->refreshTimer, &QTimer::timeout,
            [this]() { std::for_each(std::begin(this->plots),std::end(this->plots), [](auto plot){plot->replot(20);}); });
        setWidget(new QWidget(this));
        widget()->setLayout(new QVBoxLayout);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
        removeAllMargins(this);
        removeAllMargins(widget());
        setWidgetResizable(true);
    }

    ~SyncPannel() { }

    inline void setXRange(AxisRange newRange) noexcept
    {
        if (currentRange != newRange)
        {
            currentRange = newRange;
            for (auto plot : plots)
                plot->setXRange(newRange);
        }
    }

    inline void addPlot(IPlotWidget* plot)
    {
        assert(plot);
        if (plots.size())
            plots.back()->showXAxis(false);
        plots.push_back(plot);
        widget()->layout()->addWidget(plot);
        plot->setXRange(currentRange);
        connect(plot, &IPlotWidget::xRangeChanged, this, &SyncPannel::setXRange);
        connect(plot, &IPlotWidget::dataChanged, [this](){this->refreshTimer->start(20);});
    }
};
}

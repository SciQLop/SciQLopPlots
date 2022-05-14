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
#include <QTimer>
#include <QWidget>

#include <QVBoxLayout>

#include <cassert>
#include <list>

#include "../Interfaces/IPlotWidget.hpp"
#include <cpp_utils/containers/algorithms.hpp>

#include "SciQLopPlots/axis_range.hpp"

namespace SciQLopPlots
{
class SyncPannel : public QScrollArea
{
    Q_OBJECT
    std::list<interfaces::IPlotWidget*> plots;
    axis::range currentRange;
    QTimer* refreshTimer;

public:
    explicit SyncPannel(QWidget* parent = nullptr) : QScrollArea(parent), currentRange(0., 0.)
    {
        using namespace cpp_utils::containers;
        refreshTimer = new QTimer { this };
        refreshTimer->setSingleShot(true);
        connect(this->refreshTimer, &QTimer::timeout,
            [this]() { broadcast(this->plots, &interfaces::IPlotWidget::replot, 20); });
        setWidget(new QWidget(this));
        widget()->setLayout(new QVBoxLayout);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
        removeAllMargins(this);
        removeAllMargins(widget());
        setWidgetResizable(true);
    }

    virtual ~SyncPannel() { }

    inline void setXRange(axis::range newRange) noexcept
    {
        using namespace cpp_utils::containers;
        if (currentRange != newRange)
        {
            currentRange = newRange;
            broadcast(plots, &interfaces::IPlotWidget::setXRange, newRange);
        }
    }

    inline void addPlot(interfaces::IPlotWidget* plot)
    {
        assert(plot);
        if (plots.size())
            plots.back()->showXAxis(false);
        plots.push_back(plot);
        widget()->layout()->addWidget(plot);
        plot->setXRange(currentRange);
        connect(plot, &interfaces::IPlotWidget::xRangeChanged, this, &SyncPannel::setXRange);
        connect(plot, &interfaces::IPlotWidget::dataChanged, [this]() { this->refreshTimer->start(20); });
    }
};
}

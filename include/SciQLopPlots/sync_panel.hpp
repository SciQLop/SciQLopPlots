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
#include <QList>


#include <cassert>

#include "plot.hpp"
#include <cpp_utils/containers/algorithms.hpp>

#include "SciQLopPlots/axis_range.hpp"

namespace SciQLopPlots
{
class SyncPanel : public QScrollArea
{
    Q_OBJECT
    QList<PlotWidget*> _plots;
    axis::range currentRange;
    QTimer* refreshTimer;

protected:
    inline QList<PlotWidget*> plots()const {return _plots;}

public:
    explicit SyncPanel(QWidget* parent = nullptr) : QScrollArea(parent), currentRange(0., 0.)
    {
        using namespace cpp_utils::containers;
        refreshTimer = new QTimer { this };
        refreshTimer->setSingleShot(true);
        connect(this->refreshTimer, &QTimer::timeout,
            [this]() { broadcast(this->_plots, &PlotWidget::replot, 20); });
        setWidget(new QWidget(this));
        widget()->setLayout(new QVBoxLayout);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
        removeAllMargins(this);
        removeAllMargins(widget());
        setWidgetResizable(true);
    }

    virtual ~SyncPanel() { }

    inline void setXRange(axis::range newRange) noexcept
    {
        using namespace cpp_utils::containers;
        if (currentRange != newRange)
        {
            currentRange = newRange;
            broadcast(_plots, &PlotWidget::setXRange, newRange);
        }
    }

    inline void addPlot(PlotWidget* plot, int index=-1)
    {
        assert(plot);
        plot->setParent(this);
        if(index==-1 or index == _plots.size())
        {
            index = _plots.size();
            if (_plots.size())
                _plots.back()->showXAxis(false);
        }
        else
        {
            plot->showXAxis(false);
        }
        _plots.insert(index,plot);
        dynamic_cast<QVBoxLayout*>(widget()->layout())->insertWidget(index, plot);
        plot->setXRange(currentRange);
        connect(plot, &interfaces::IPlotWidget::xRangeChanged, this, &SyncPanel::setXRange);
        connect(plot, &interfaces::IPlotWidget::dataChanged,
            [this]() { this->refreshTimer->start(20); });

    }

    inline int indexOf(QWidget* wdgt)
    {
        assert(wdgt);
        return dynamic_cast<QVBoxLayout*>(widget()->layout())->indexOf(wdgt);
    }

    inline std::size_t count(){return std::size(_plots);}
};
}

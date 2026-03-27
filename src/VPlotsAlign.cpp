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
#include "SciQLopPlots/SciQLopPlot.hpp"

#include "SciQLopPlots/MultiPlots/VPlotsAlign.hpp"

#include <QEvent>
#include <limits>

void VPlotsAlign::_recompute_margins()
{
    int max_left_margin = 0;
    int max_right_pos = std::numeric_limits<int>::max();
    for (auto& _p : _plots)
    {
        if (auto p = qobject_cast<SciQLopPlot*>(_p))
        {
            auto ar = p->qcp_plot()->axisRect();
            if (p->width() <= 0 || p->height() <= 0
                || ar->rect().width() <= 0 || ar->rect().height() <= 0)
                continue;
            int left_margin = ar->calculateAutoMargin(QCP::MarginSide::msLeft);
            int cmw = 0;
            if (p->has_colormap())
            {
                cmw = p->color_scale()->outerRect().width();
                cmw += ar->calculateAutoMargin(QCP::MarginSide::msRight);
            }
            max_left_margin = std::max(max_left_margin, left_margin);
            max_right_pos = std::min(max_right_pos, std::max(0, p->width() - cmw));
        }
    }
    for (auto& _p : _plots)
    {
        if (auto p = qobject_cast<SciQLopPlot*>(_p))
        {
            auto ar = p->qcp_plot()->axisRect();
            if (p->width() <= 0 || p->height() <= 0
                || ar->rect().width() <= 0 || ar->rect().height() <= 0)
                continue;
            int new_right_margin = std::max(0, p->width() - max_right_pos);
            if (p->has_colormap())
            {
                new_right_margin = std::max(0, new_right_margin - p->color_scale()->outerRect().width());
            }
            else
            {
                new_right_margin += p->qcp_plot()->plotLayout()->columnSpacing();
            }
            auto new_margins
                = QMargins(max_left_margin, ar->calculateAutoMargin(QCP::MarginSide::msTop),
                           new_right_margin, ar->calculateAutoMargin(QCP::MarginSide::msBottom));
            if (ar->minimumMargins() != new_margins)
            {
                ar->setMinimumMargins(new_margins);
                p->replot();
            }
        }
    }
}

VPlotsAlign::VPlotsAlign(QObject* parent) : SciQLopPlotCollectionBehavior(parent)
{
    m_debounce_timer = new QTimer(this);
    m_debounce_timer->setSingleShot(true);
    m_debounce_timer->setInterval(16);
    connect(m_debounce_timer, &QTimer::timeout, this, &VPlotsAlign::_recompute_margins);
}

void VPlotsAlign::updatePlotList(const QList<QPointer<SciQLopPlotInterface>>& plots)
{
    SciQLopPlotCollectionBehavior::_update_collection(
        _plots,
        plots,
        [this](SciQLopPlotInterface* plot)
        {
            if (auto p = qobject_cast<SciQLopPlot*>(plot))
            {
                connect(p, &SciQLopPlot::y_axis_range_changed, this,
                        [this](SciQLopPlotRange) { m_debounce_timer->start(); });
                connect(p, &SciQLopPlot::y2_axis_range_changed, this,
                        [this](SciQLopPlotRange) { m_debounce_timer->start(); });
                connect(p, &SciQLopPlot::z_axis_range_changed, this,
                        [this](SciQLopPlotRange) { m_debounce_timer->start(); });
                p->installEventFilter(this);
            }
        },
        [this](SciQLopPlotInterface* plot)
        {
            if (auto p = qobject_cast<SciQLopPlot*>(plot))
            {
                p->removeEventFilter(this);
                this->disconnect(p);
            }
        });
    if (!_plots.isEmpty())
    {
        _recompute_margins();
    }
}

bool VPlotsAlign::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Resize)
        m_debounce_timer->start();
    return false;
}

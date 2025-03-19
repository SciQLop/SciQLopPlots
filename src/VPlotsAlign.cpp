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

class UnProtectedQCPAxisRect : public QCPAxisRect
{
public:
    inline int calculateAutoMargin(QCP::MarginSide side)
    {
        return QCPAxisRect::calculateAutoMargin(side);
    }
};

void VPlotsAlign::_recompute_margins()
{
    std::size_t max_left_margin = 0UL;
    std::size_t max_right_pos = 1000000000UL;
    for (auto& _p : _plots)
    {
        if (auto p = qobject_cast<SciQLopPlot*>(_p))
        {

            auto ar = reinterpret_cast<UnProtectedQCPAxisRect*>(p->qcp_plot()->axisRect());
            std::size_t left_margin = ar->calculateAutoMargin(QCP::MarginSide::msLeft);
            std::size_t cmw = 0UL;
            if (p->has_colormap())
            {

                cmw = p->color_scale()->outerRect().width();
                cmw += ar->calculateAutoMargin(QCP::MarginSide::msRight);
            }
            max_left_margin = std::max(max_left_margin, left_margin);
            max_right_pos = std::min(max_right_pos, p->width() - cmw);
        }
    }
    for (auto& _p : _plots)
    {
        if (auto p = qobject_cast<SciQLopPlot*>(_p))
        {
            auto ar = reinterpret_cast<UnProtectedQCPAxisRect*>(p->qcp_plot()->axisRect());
            std::size_t new_right_margin = p->width() - max_right_pos;
            if (p->has_colormap())
            {
                new_right_margin -= p->color_scale()->outerRect().width();
            }
            else
            {
                new_right_margin += p->qcp_plot()->plotLayout()->columnSpacing();
            }
            auto new_margins
                = QMargins(max_left_margin, ar->calculateAutoMargin(QCP::MarginSide::msTop),
                           new_right_margin, ar->calculateAutoMargin(QCP::MarginSide::msTop));
            if (ar->minimumMargins() != new_margins)
            {
                ar->setMinimumMargins(new_margins);
                p->replot();
            }
        }
    }
}

VPlotsAlign::VPlotsAlign(QObject* parent) : SciQLopPlotCollectionBehavior(parent) { }

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
                        [this](SciQLopPlotRange) { _recompute_margins(); });
                connect(p, &SciQLopPlot::y2_axis_range_changed, this,
                        [this](SciQLopPlotRange) { _recompute_margins(); });
                connect(p, &SciQLopPlot::z_axis_range_changed, this,
                        [this](SciQLopPlotRange) { _recompute_margins(); });
            }
        },
        [this](SciQLopPlotInterface* plot)
        {
            if (auto p = qobject_cast<SciQLopPlot*>(plot))
            {

                this->disconnect(p);
            }
        });
    if (!_plots.isEmpty())
    {
        _recompute_margins();
    }
}

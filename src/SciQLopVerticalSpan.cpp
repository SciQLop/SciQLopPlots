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
#include "SciQLopPlots/SciQLopVerticalSpan.hpp"

void VerticalSpanBorder::move(double dx, double dy)
{
    this->point1->setPixelPosition(
        { this->point1->pixelPosition().x() + dx, this->point1->pixelPosition().y() });

    this->point2->setPixelPosition(
        { this->point2->pixelPosition().x() + dx, this->point2->pixelPosition().y() });

    this->replot();
    emit moved(this->point1->coords().x());
}

VerticalSpan::VerticalSpan(QCustomPlot *plot, QCPRange horizontal_range, bool do_not_replot, bool immediate_replot)
        : SciQLopPlotItem { plot }
        , _border1 { new VerticalSpanBorder { plot, horizontal_range.lower, true } }
        , _border2 { new VerticalSpanBorder { plot, horizontal_range.upper, do_not_replot } }
{
    this->setLayer(Constants::LayersNames::Spans);
    this->set_color(QColor(0, 255, 0, 40));
    this->set_auto_extend_vertically();
    this->setSelectable(true);
    this->setMovable(true);
    this->setVisible(true);
    connect(this->_border1, &VerticalSpanBorder::moved, this,
        [this](double x)
        {
            this->set_left_pos(this->_lower_border()->position());
            this->set_right_pos(this->_upper_border()->position());
            this->replot();
            emit this->range_changed(this->range());
        });

    connect(this->_border2, &VerticalSpanBorder::moved, this,
        [this](double x)
        {
            this->set_left_pos(this->_lower_border()->position());
            this->set_right_pos(this->_upper_border()->position());
            this->replot();
            emit this->range_changed(this->range());
        });

    this->set_left_pos(std::min(horizontal_range.lower, horizontal_range.upper));
    this->set_right_pos(std::max(horizontal_range.lower, horizontal_range.upper));
    if (!do_not_replot)
        this->replot(immediate_replot);
}

void VerticalSpan::set_range(const QCPRange horizontal_range)
{
    if (this->range() != horizontal_range)
    {
        this->set_left_pos(std::min(horizontal_range.lower, horizontal_range.upper));
        this->set_right_pos(std::max(horizontal_range.lower, horizontal_range.upper));
        this->replot();
        emit range_changed(horizontal_range);
    }
}

void VerticalSpan::move(double dx, double dy)
{
    if (dx != 0.)
    {
        auto x_axis = this->parentPlot()->axisRect()->rangeDragAxis(Qt::Horizontal);
        this->set_left_pos(x_axis->pixelToCoord(this->topLeft->pixelPosition().x() + dx));
        this->set_right_pos(x_axis->pixelToCoord(this->bottomRight->pixelPosition().x() + dx));
        this->replot();
        emit this->range_changed(this->range());
    }
}

double VerticalSpan::selectTest(const QPointF &pos, bool onlySelectable, QVariant *details) const
{
    auto left = std::min(this->left->pixelPosition().x(), this->right->pixelPosition().x());
    auto right = std::max(this->left->pixelPosition().x(), this->right->pixelPosition().x());
    if (pos.y() <= this->top->pixelPosition().y()
        or pos.y() >= this->bottom->pixelPosition().y())
        return -1;
    auto width = right - left;
    {
        auto left_border_distance
            = this->_lower_border()->selectTest(pos, onlySelectable, details);
        if (left_border_distance != -1. and left_border_distance <= std::max(10., width * 0.1))
            return -1.;
    }
    {
        auto right_border_distance
            = this->_upper_border()->selectTest(pos, onlySelectable, details);
        if (right_border_distance != -1.
            and right_border_distance <= std::max(10., width * 0.1))
            return -1.;
    }

    if (pos.x() <= right and pos.x() >= left)
    {
        return 0;
    }
    else
    {

        return std::min(abs(pos.x() - left), abs(pos.x() - right));
    }
}

SciQLopVerticalSpan::SciQLopVerticalSpan(QCustomPlot *plot, QCPRange horizontal_range, bool do_not_replot)
        : _impl { new VerticalSpan { plot, horizontal_range, do_not_replot } }
{
    connect(
        this->_impl, &VerticalSpan::range_changed, this, &SciQLopVerticalSpan::range_changed);

    connect(this->_impl, &VerticalSpan::selectionChanged, this,
        &SciQLopVerticalSpan::selectionChanged);
}

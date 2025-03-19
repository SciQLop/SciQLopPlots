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
#include <SciQLopPlots/SciQLopPlot.hpp>

#include "SciQLopPlots/Items/SciQLopVerticalSpan.hpp"


void impl::VerticalSpanBorder::move(double dx, double dy)
{
    this->point1->setPixelPosition(
        { this->point1->pixelPosition().x() + dx, this->point1->pixelPosition().y() });

    this->point2->setPixelPosition(
        { this->point2->pixelPosition().x() + dx, this->point2->pixelPosition().y() });

    this->replot();
    emit moved(this->point1->coords().x());
}

void impl::VerticalSpan::border1_selection_changed(bool select)
{
    if (_border1 == _lower_border())
    {
        Q_EMIT lower_border_selection_changed(select);
    }
    else
    {
        Q_EMIT upper_border_selection_changed(select);
    }
}

void impl::VerticalSpan::border2_selection_changed(bool select)
{
    if (_border2 == _lower_border())
    {
        Q_EMIT lower_border_selection_changed(select);
    }
    else
    {
        Q_EMIT upper_border_selection_changed(select);
    }
}

impl::VerticalSpan::VerticalSpan(
    QCustomPlot* plot, SciQLopPlotRange horizontal_range, bool do_not_replot, bool immediate_replot)
        : impl::SciQLopPlotItem<QCPItemRect>{ plot }
        , _border1 { new impl::VerticalSpanBorder { plot, horizontal_range.sorted().start(), true } }
        , _border2 { new impl::VerticalSpanBorder {
              plot, horizontal_range.sorted().stop(), do_not_replot } }
{
    this->setLayer(Constants::LayersNames::Spans);
    this->set_color(QColor(0, 255, 0, 40));
    this->set_auto_extend_vertically();
    this->setSelectable(true);
    this->setMovable(true);
    this->setVisible(true);
    connect(this->_border1, &impl::VerticalSpanBorder::moved, this,
        [this](double x)
        {
            this->set_left_pos(this->_lower_border()->position());
            this->set_right_pos(this->_upper_border()->position());
            this->replot();
            emit this->range_changed(this->range());
        });

    connect(this->_border2, &impl::VerticalSpanBorder::moved, this,
        [this](double x)
        {
            this->set_left_pos(this->_lower_border()->position());
            this->set_right_pos(this->_upper_border()->position());
            this->replot();
            emit this->range_changed(this->range());
        });

    connect(this->_border1, &impl::VerticalSpanBorder::selectionChanged, this,
        &impl::VerticalSpan::border1_selection_changed);
    connect(this->_border2, &impl::VerticalSpanBorder::selectionChanged, this,
        &impl::VerticalSpan::border2_selection_changed);
    auto sorted = horizontal_range.sorted();
    this->set_left_pos(sorted.start());
    this->set_right_pos(sorted.stop());
    if (!do_not_replot)
        this->replot(immediate_replot);
}

void impl::VerticalSpan::keyPressEvent(QKeyEvent* event)
{
    if (this->selected())
    {
        if (event->key() == Qt::Key_Delete)
        {
            Q_EMIT delete_requested();
        }
    }
}

void impl::VerticalSpan::set_range(const SciQLopPlotRange horizontal_range)
{
    auto sorted = horizontal_range.sorted();
    if (this->range() != sorted)
    {
        this->set_left_pos(sorted.start());
        this->set_right_pos(sorted.stop());
        this->replot();
        emit range_changed(sorted);
    }
}

void impl::VerticalSpan::move(double dx, double dy)
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

double impl::VerticalSpan::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    auto left = std::min(this->left->pixelPosition().x(), this->right->pixelPosition().x());
    auto right = std::max(this->left->pixelPosition().x(), this->right->pixelPosition().x());
    if (pos.y() <= this->top->pixelPosition().y() or pos.y() >= this->bottom->pixelPosition().y())
        return -1;
    auto width = right - left;
    {
        auto left_border_distance = this->_lower_border()->selectTest(pos, onlySelectable, details);
        if (left_border_distance != -1. and left_border_distance <= (width * 0.1))
            return -1.;
    }
    {
        auto right_border_distance
            = this->_upper_border()->selectTest(pos, onlySelectable, details);
        if (right_border_distance != -1. and right_border_distance <= (width * 0.1))
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

void impl::VerticalSpan::select_lower_border(bool selected)
{
    if (!_border1.isNull() && !_border2.isNull())
    {
        if (this->_lower_border()->selected() != selected or _lower_border_selected != selected)
        {
            _lower_border_selected = selected;
            this->_lower_border()->setSelected(selected);
            Q_EMIT lower_border_selection_changed(selected);
        }
    }
}

void impl::VerticalSpan::select_upper_border(bool selected)
{
    if (!_border1.isNull() && !_border2.isNull())
    {
        if (this->_upper_border()->selected() != selected or _upper_border_selected != selected)
        {
            _upper_border_selected = selected;
            this->_upper_border()->setSelected(selected);
            Q_EMIT upper_border_selection_changed(selected);
        }
    }
}
SciQLopVerticalSpan::SciQLopVerticalSpan(SciQLopPlot* plot, SciQLopPlotRange horizontal_range,
    QColor color, bool read_only, bool visible, const QString& tool_tip)
        :SciQLopRangeItemInterface { plot },
        _impl { new impl::VerticalSpan { plot->qcp_plot(), horizontal_range, true } }
{
    set_color(color);
    set_read_only(read_only);
    set_visible(visible);
    set_tool_tip(tool_tip);
    connect(this->_impl.data(), &impl::VerticalSpan::range_changed, this,
        &SciQLopVerticalSpan::range_changed);

    connect(this->_impl.data(), &impl::VerticalSpan::selectionChanged, this,
        &SciQLopVerticalSpan::selectionChanged);

    connect(this->_impl.data(), &impl::VerticalSpan::lower_border_selection_changed, this,
        &SciQLopVerticalSpan::lower_border_selection_changed);
    connect(this->_impl.data(), &impl::VerticalSpan::upper_border_selection_changed, this,
        &SciQLopVerticalSpan::upper_border_selection_changed);

    connect(this->_impl.data(), &impl::VerticalSpan::delete_requested, this,
        &SciQLopVerticalSpan::delete_requested);
}

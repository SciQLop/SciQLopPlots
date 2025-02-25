/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Items/SciQLopStraightLines.hpp"

void StraightLine::move(double dx, double dy)
{
    if (m_orientation == Qt::Orientation::Vertical)
    {
        this->point1->setPixelPosition({this->point1->pixelPosition().x() + dx, this->point1->pixelPosition().y()});
        this->point2->setPixelPosition({this->point2->pixelPosition().x() + dx, this->point2->pixelPosition().y()});
        emit moved(this->point1->key());
    }
    else
    {
        this->point1->setPixelPosition({this->point1->pixelPosition().x(), this->point1->pixelPosition().y() + dy});
        this->point2->setPixelPosition({this->point2->pixelPosition().x(), this->point2->pixelPosition().y() + dy});
        emit moved(this->point1->value());
    }
    this->replot();
}

void StraightLine::set_position(double pos)
{
    if (m_orientation == Qt::Orientation::Vertical)
    {
        this->point1->setCoords(pos, 0);
        this->point2->setCoords(pos, 1);
    }
    else
    {
        this->point1->setCoords(0, pos);
        this->point2->setCoords(1, pos);
    }
    this->replot();
}

double StraightLine::position() const
{
    if (m_orientation == Qt::Orientation::Vertical)
    {
        return this->point1->key();
    }
    else
    {
        return this->point1->value();
    }
}

void StraightLine::set_color(const QColor &color)
{
    auto pen = this->pen();
    pen.setColor(color);
    this->setPen(pen);
}

QColor StraightLine::color() const
{
    return this->pen().color();
}

void StraightLine::set_line_width(double width)
{
    auto pen = this->pen();
    pen.setWidthF(width);
    this->setPen(pen);
}

double StraightLine::line_width() const
{
    return this->pen().width();
}

void SciQLopStraightLine::set_position(double pos)
{
    m_line->set_position(pos);
}



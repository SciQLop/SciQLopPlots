/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/graph.hpp"

void SciQLopPlots::ColorMapGraph::plot(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> z){}

void SciQLopPlots::MultiLineGraph::plot_ptr(double *x, double *y, std::size_t x_size, std::size_t y_size, enums::DataOrder order)
{
    m_plot_widget()->plot(m_indexes, x, y, x_size, y_size, order);
}

void SciQLopPlots::LineGraph::plot_ptr(double *x, double *y, std::size_t x_size, std::size_t y_size)
{
    m_plot_widget()->plot(m_index, x, y, x_size, y_size);
}

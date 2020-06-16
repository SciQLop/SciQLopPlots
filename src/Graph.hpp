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

#include "SciQLopPlot.hpp"

#include <types/detectors.hpp>

#include <channels/channels.hpp>

#include <thread>

namespace SciQLopPlots
{

template <typename data_type, typename plot_t>
struct Grapgh
{
    channels::channel<AxisRange, 1, channels::full_policy::overwrite_last> transformations_out;
    channels::channel<data_type, 1, channels::full_policy::overwrite_last> data_in;
    Grapgh(int index, plot_t* plot) : m_plot { plot }, m_graphIndex { index }
    {
        QObject::connect(
            plot, &plot_t::xRangeChanged, [this](auto range) { transformations_out << range; });
        QObject::connect(
            plot, &plot_t::closed, [this]() {data_in.close();transformations_out.close(); });
        m_updateThread = std::thread([this]() {
            while (!data_in.closed())
            {
                auto data = data_in.take();
                if(!data_in.closed())
                    SciQLopPlots::plot(*m_plot, m_graphIndex, std::move(data));
            }
        });
    }

    ~Grapgh()
    {
        data_in.close();
        transformations_out.close();
        m_updateThread.join();
    }

private:
    plot_t* m_plot = nullptr;
    std::thread m_updateThread;
    int m_graphIndex;
};

template <typename data_type, typename PlotImpl>
Grapgh<data_type, PlotWidget<PlotImpl>> add_graph(
    PlotWidget<PlotImpl>* plot, QColor color = Qt::blue)
{
    return Grapgh<data_type, PlotWidget<PlotImpl>>(plot->addGraph(color), plot);
}

}

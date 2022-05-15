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

#include "../Interfaces/PlotWidget.hpp"

#include <cpp_utils/types/detectors.hpp>
#include <cpp_utils/cpp_utils.hpp>

#include <channels/channels.hpp>

#include <memory>
#include <thread>

#include "SciQLopPlots/axis_range.hpp"

namespace SciQLopPlots
{

namespace details
{
    template <typename data_type, typename plot_t>
    struct Graph
    {
        using channel_tag = struct channel_tag;
        channels::channel<axis::range, 1, channels::full_policy::overwrite_last>
            transformations_out;

        Graph(std::vector<int> indexes, plot_t* plot) : m_plot { plot }, m_graphIndexes { indexes }
        {
            QObject::connect(
                plot, &plot_t::xRangeChanged, [this](auto range) { transformations_out << range; });
            QObject::connect(plot, &plot_t::closed,
                [this]()
                {
                    this->close();
                    this->m_plot = nullptr;
                });
            m_updateThread = std::thread(
                [this]()
                {
                    while (!m_data_in.closed())
                    {
                        if (auto maybeData = m_data_in.take(); m_plot && maybeData)
                            SciQLopPlots::plot(*m_plot, m_graphIndexes, std::move(*maybeData));
                    }
                });
        }

        void add(const data_type& data) { m_data_in.add(data); }

        void add(data_type&& data) { m_data_in.add(std::move(data)); }

        inline bool closed() { return m_data_in.closed() or transformations_out.closed(); }

        inline void close()
        {
            m_data_in.close();
            transformations_out.close();
        }

        ~Graph()
        {
            m_data_in.close();
            transformations_out.close();
            m_updateThread.join();
        }

    private:
        channels::channel<data_type, 1, channels::full_policy::overwrite_last> m_data_in;
        plot_t* m_plot = nullptr;
        std::thread m_updateThread;
        std::vector<int> m_graphIndexes;
    };

    inline const auto colors = std::vector<QColor>{Qt::blue, Qt::red, Qt::green, Qt::black, Qt::yellow, Qt::gray};
}

template <typename data_type, typename plot_t>
struct Graph
{
private:
    std::shared_ptr<details::Graph<data_type, plot_t>> m_impl;

public:
    using channel_tag = struct channel_tag;
    using in_value_type = data_type;
    channels::channel<axis::range, 1, channels::full_policy::overwrite_last> transformations_out;
    Graph(std::vector<int> indexes, plot_t* plot)
            : m_impl { new details::Graph<data_type, plot_t>(indexes, plot) }
            , transformations_out { m_impl->transformations_out }
    {
    }

    Graph(int index, plot_t* plot)
            : m_impl { new details::Graph<data_type, plot_t>({index}, plot) }
            , transformations_out { m_impl->transformations_out }
    {
    }

    Graph(const Graph& other)
            : m_impl { other.m_impl }, transformations_out { m_impl->transformations_out }
    {
    }

    Graph(Graph&& other)
            : m_impl { std::move(other.m_impl) }
            , transformations_out { m_impl->transformations_out }
    {
    }

    inline Graph& operator<<(data_type&& data)
    {
        m_impl->add(std::move(data));
        return *this;
    }

    inline Graph& operator<<(const data_type& data)
    {
        m_impl->add(data);
        return *this;
    }

    inline Graph& add(data_type&& data)
    {
        m_impl->add(std::move(data));
        return *this;
    }

    inline Graph& add(const data_type& data)
    {
        m_impl->add(data);
        return *this;
    }

    inline bool closed() { return m_impl->closed(); }

    inline void close() { m_impl->close(); }

    ~Graph() { }
};


template <typename data_type, typename PlotImpl>
Graph<data_type, interfaces::PlotWidget<PlotImpl>> add_graph(
    interfaces::PlotWidget<PlotImpl>* plot, QColor color = Qt::blue)
{
    return Graph<data_type, interfaces::PlotWidget<PlotImpl>>(plot->addGraph(color), plot);
}

template <typename data_type, typename PlotImpl>
Graph<data_type, interfaces::PlotWidget<PlotImpl>> add_graph(
    interfaces::PlotWidget<PlotImpl>* plot, std::size_t count=1, std::vector<QColor> colors=details::colors)
{
    std::vector<int> graphs;
    count * [&graphs, plot, color = std::cbegin(colors)]() mutable
    {
        graphs.push_back(plot->addGraph(*color));
        color++;
    };
    return Graph<data_type, interfaces::PlotWidget<PlotImpl>>(graphs, plot);
}


template <typename data_type, typename PlotImpl>
Graph<data_type, interfaces::PlotWidget<PlotImpl>> add_colormap(
    interfaces::PlotWidget<PlotImpl>* plot)
{
    return Graph<data_type, interfaces::PlotWidget<PlotImpl>>(plot->addColorMap(), plot);
}

}

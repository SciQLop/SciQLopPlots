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

#include "SciQLopPlots/SciQLopGraph.hpp"


SciQLopGraph::SciQLopGraph(
    QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis, QStringList labels)
        : QObject(parent), _keyAxis { keyAxis }, _valueAxis { valueAxis }
{
    this->_create_graphs(labels);
}

SciQLopGraph::~SciQLopGraph() { }

void SciQLopGraph::setData(NpArray_view &&x, NpArray_view &&y)
{
    _x = std::move(x);
    _y = std::move(y);
    if (_x.flat_size() != 0)
    {
        const auto line_cnt = _y.flat_size() / x.flat_size();
        assert(line_cnt == std::size(_graphs));
        for (auto index = 0UL; index < line_cnt; index++)
        {
            QVector<QCPGraphData> graph_data(_x.flat_size());
            const auto x_data = _x.data();
            const auto y_data = _y.data() + (index * _x.flat_size());
            for (auto i = 0UL; i < x.flat_size(); i++)
            {
                graph_data[i] = { x_data[i], y_data[i] };
            }
            _graphs[index]->data()->set(std::move(graph_data), true);
        }
    }
}

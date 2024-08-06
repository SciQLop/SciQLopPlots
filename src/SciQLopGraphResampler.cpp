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

#include "SciQLopPlots/Plotables/SciQLopGraphResampler.hpp"


GraphResampler::GraphResampler(SciQLopGraph::DataOrder dataOrder, std::size_t line_cnt)
        : AbstractResampler1d { dataOrder, line_cnt }
{
}


void AbstractResampler1d::_resample_slot(const QCPRange newRange)
{
    QMutexLocker locker(&_data_mutex);
    {
        QMutexLocker locker(&_next_data_mutex);
        _data = std::move(_next_data);
    }
    this->_resample(std::move(_data.x), std::move(_data.y), newRange);
    emit this->refreshPlot();
}

AbstractResampler1d::AbstractResampler1d(SciQLopGraph::DataOrder dataOrder, std::size_t line_cnt)
        : _dataOrder { dataOrder }, _line_cnt { line_cnt }
{

    connect(this, &AbstractResampler1d::_resample_sig, this, &AbstractResampler1d::_resample_slot,
        Qt::QueuedConnection);
}

void AbstractResampler1d::resample(const QCPRange newRange)
{
    emit this->_resample_sig(newRange);
}

void GraphResampler::_resample(Array_view&& x, Array_view&& y, const QCPRange newRange)
{
    if (x.data() != nullptr && x.flat_size() > 0)
    {
        const auto start_x = std::upper_bound(x.data(), x.data() + x.flat_size(), newRange.lower);
        const auto end_x = std::lower_bound(x.data(), x.data() + x.flat_size(), newRange.upper);
        const auto x_window_size = end_x - start_x;
        if (x_window_size > 0)
        {
            const auto y_incr
                = (dataOrder() == SciQLopGraph::DataOrder::xFirst) ? 1UL : line_count();
            for (auto line_index = 0UL; line_index < line_count(); line_index++)
            {
                const auto start_y = y.data() + y_incr * (start_x - x.data())
                    + (line_index
                        * ((dataOrder() == SciQLopGraph::DataOrder::xFirst) ? x.flat_size() : 1));
                if (x_window_size > 10000)
                {
                    emit this->setGraphData(
                        line_index, ::resample<10000>(start_x, start_y, x_window_size, y_incr));
                }
                else
                {
                    emit this->setGraphData(
                        line_index, copy_data(start_x, start_y, x_window_size, y_incr));
                }
            }
        }
        x.release();
        y.release();
    }
}

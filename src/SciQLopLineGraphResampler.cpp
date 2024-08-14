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

#include "SciQLopPlots/Plotables/SciQLopLineGraphResampler.hpp"
#include <iostream>

LineGraphResampler::LineGraphResampler(std::size_t line_cnt) : AbstractResampler1d { line_cnt } { }


void AbstractResampler1d::_resample_slot()
{
    {
        QMutexLocker locker(&_data_mutex);
        {
            QMutexLocker locker(&_next_data_mutex);
            _data = _next_data;
            _next_data.new_data = false;
        }
        this->_resample(_data.x, _data.y, _data._plot_range, _data.new_data);
    }
}

AbstractResampler1d::AbstractResampler1d(std::size_t line_cnt) : _line_cnt { line_cnt }
{
    connect(this, &AbstractResampler1d::_resample_sig, this, &AbstractResampler1d::_resample_slot,
        Qt::QueuedConnection);
}

void AbstractResampler1d::resample(const QCPRange new_range)
{
    {
        QMutexLocker locker(&_next_data_mutex);
        _next_data._plot_range = new_range;
    }
    emit _resample_sig();
}

void LineGraphResampler::_resample(
    const PyBuffer& x, const PyBuffer& y, const QCPRange new_range, bool new_data)
{
    if (x.data() != nullptr && x.flat_size() > 0)
    {
        const auto start_x = std::upper_bound(x.data(), x.data() + x.flat_size(), new_range.lower);
        const auto end_x = std::lower_bound(x.data(), x.data() + x.flat_size(), new_range.upper);
        const auto x_window_size = end_x - start_x;
        if (x_window_size > 0)
        {
            const auto y_incr = 1UL;
            for (auto line_index = 0UL; line_index < line_count(); line_index++)
            {
                std::size_t start_y_index = std::distance(x.data(), start_x);
                const auto start_y
                    = y.data() + y_incr * (start_x - x.data()) + (line_index * x.flat_size());
                auto y_view = y.view(start_y_index);
                if (x_window_size > 10000)
                {
                    emit this->setGraphData(
                        line_index, ::resample<10000>(start_x, *y_view, line_index, x_window_size));
                }
                else
                {
                    emit this->setGraphData(
                        line_index, copy_data(start_x, *y_view, line_index, x_window_size));
                }
            }
        }
    }
}

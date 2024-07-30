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

#include "SciQLopPlots/SciQLopColorMapResampler.hpp"
#include <cpp_utils/containers/algorithms.hpp>


std::vector<double> ColormapResampler::_optimal_y_scale(
    const Array_view& x, const Array_view& y, QCPAxis::ScaleType scale_type)
{
    using namespace cpp_utils;
    std::vector<double> new_y(this->_max_y_size);
    auto y_min = *containers::min(y);
    auto y_max = *containers::max(y);

    if (scale_type == QCPAxis::ScaleType::stLinear)
    {
        std::generate(std::begin(new_y), std::end(new_y),
            [start = y_min, step = (y_max - y_min) / this->_max_y_size, i = 0]() mutable
            {
                i++;
                return start + i * step;
            });
    }
    else if (scale_type == QCPAxis::ScaleType::stLogarithmic)
    {
        std::generate(std::begin(new_y), std::end(new_y),
            [start = y_min, step = (y_max - y_min) / this->_max_y_size, i = 0]() mutable
            {
                i++;
                return start * std::pow(10, i * step);
            });
    }
    return new_y;
}

QCPColorMapData* ColormapResampler::_setDataLinear(
    const Array_view& x, const Array_view& y, const Array_view& z)
{

    using namespace cpp_utils;
    auto data = new QCPColorMapData(std::size(x), std::size(y),
        { *containers::min(x), *containers::max(x) }, { *containers::min(y), *containers::max(y) });
    auto it = std::cbegin(z);
    for (const auto i : x)
    {
        for (const auto j : y)
        {
            if (it != std::cend(z))
            {
                data->setData(i, j, *it);
                it++;
            }
        }
    }
    return data;
}

QCPColorMapData* ColormapResampler::_setDataLog(
    const Array_view& x, const Array_view& y, const Array_view& z)
{
    using namespace cpp_utils;
    auto data = new QCPColorMapData(std::size(x), std::size(y),
        { *containers::min(x), *containers::max(x) }, { *containers::min(y), *containers::max(y) });
    auto it = std::cbegin(z);
    for (auto i = 0UL; i < std::size(x); i++)
    {
        for (auto j = 0UL; j < std::size(y); j++)
        {
            if (it != std::cend(z))
            {
                data->setCell(i, j, *it);
                it++;
            }
        }
    }
    return data;
}

void ColormapResampler::_resample_slot(const QCPRange &newRange)
{
    {
        QMutexLocker lock { &_range_mutex };
        if (this->_data_x_range != newRange)
            return;
    }
    _mutex.lock();
    auto x = std::move(_x);
    auto y = std::move(_y);
    auto z = std::move(_z);
    _mutex.unlock();
    if (std::size(x) && std::size(y) && std::size(z))
    {
        if (this->_scale_type == QCPAxis::stLinear)
        {
            emit this->refreshPlot(this->_setDataLinear(x, y, z));
        }
        else
        {
            emit this->refreshPlot(this->_setDataLog(x, y, z));
        }
    }
    else
    {
        emit this->refreshPlot(new QCPColorMapData(0, 0, { 0., 0. }, { 0., 0. }));
    }
}

ColormapResampler::ColormapResampler(QCPAxis::ScaleType scale_type) : _scale_type { scale_type }
{
    connect(this, &ColormapResampler::_resample_sig, this, &ColormapResampler::_resample_slot,
        Qt::QueuedConnection);
}

void ColormapResampler::resample(const QCPRange &newRange) { emit this->_resample_sig(newRange); }

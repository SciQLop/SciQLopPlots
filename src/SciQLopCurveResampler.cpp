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
#include "SciQLopPlots/Plotables/SciQLopCurveResampler.hpp"

QVector<QCPCurveData> curve_copy_data(
    const double* x, const double* y, std::size_t x_size, const int y_incr)
{
    QVector<QCPCurveData> data(x_size);
    const double* current_y_it = y;
    for (auto i = 0UL; i < x_size; i++)
    {
        data[i] = QCPCurveData { static_cast<double>(i), x[i], *current_y_it };
        current_y_it += y_incr;
    }
    return data;
}


void CurveResampler::_resample(
    const PyBuffer& x, const PyBuffer& y, const QCPRange newRange, bool new_data)
{
    if (x.data() != nullptr && x.flat_size() > 0 && new_data)
    {


        const auto y_incr = (data_order() == ::DataOrder::RowMajor) ? 1UL : line_count();
        for (auto line_index = 0UL; line_index < line_count(); line_index++)
        {
            const auto count = std::size(x);
            const auto start_y = y.data()
                + (line_index * ((data_order() == ::DataOrder::RowMajor) ? x.flat_size() : 1));
            emit this->setGraphData(line_index, curve_copy_data(x.data(), start_y, count, y_incr));
        }
    }
}

CurveResampler::CurveResampler(::DataOrder dataOrder, std::size_t line_cnt)
        : AbstractResampler1d { dataOrder, line_cnt }
{
}

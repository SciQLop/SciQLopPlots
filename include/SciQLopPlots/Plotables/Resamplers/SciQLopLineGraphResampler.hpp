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
#pragma once

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "AbstractResampler.hpp"

#include <qcustomplot.h>


template <std::size_t dest_size>
static inline QVector<QCPGraphData> resample(const XYView& view, std::size_t column_index)
{
    static_assert(dest_size % 2 == 0);
    const auto x_0 = view.x(0);
    const auto x_1 = view.x(std::size(view) - 1);
    auto dx = 2. * (x_1 - x_0) / dest_size;
    QVector<QCPGraphData> data(dest_size);
    std::size_t data_index = 0UL;
    std::size_t view_index = 0UL;

    for (auto bucket = 0UL; bucket < dest_size / 2; bucket++)
    {
        {
            const auto bucket_start_x = view.x(view_index);
            double bucket_max_x = std::min(bucket_start_x + dx, x_1);
            double max_value = std::nan("");
            double min_value = std::nan("");
            auto current_x = bucket_start_x;
            while (current_x < bucket_max_x && view_index < std::size(view))
            {
                current_x = view.x(view_index);
                auto v = view.y(view_index++, column_index);
                max_value = std::fmax(max_value, v);
                min_value = std::fmin(min_value, v);
            }
            data[data_index++] = QCPGraphData { bucket_start_x, min_value };
            data[data_index++] = QCPGraphData { bucket_start_x + dx / 2., max_value };
        }
    }
    return data;
}


struct LineGraphResampler : public AbstractResampler1d
{
    Q_OBJECT

    void _resample_impl(
        const PyBuffer& x, const PyBuffer& y, const QCPRange new_range, bool new_data) override;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(QList<QVector<QCPGraphData>> data);
#endif

    LineGraphResampler(std::size_t line_cnt);
};

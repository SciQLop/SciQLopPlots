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
    auto dx = 2. * (x_1 - x_0) / static_cast<double>(dest_size);
    QVector<QCPGraphData> data(dest_size);
    {
        auto key = x_0;
        auto dest_index = 0UL;
        auto next_x = x_0 + dx;
        data[0].key = key;
        data[0].value = std::nan("");
        data[1].key = key + dx / 2.;
        data[1].value = std::nan("");
        for (auto view_index = 0UL; view_index < std::size(view); view_index++)
        {
            auto x = view.x(view_index);
            auto y = view.y(view_index, column_index);
            while (x > next_x && dest_index < dest_size - 4)
            {
                dest_index += 2;
                data[dest_index].key = next_x;
                data[dest_index].value = std::nan("");
                data[dest_index + 1].key = next_x + dx / 2.;
                data[dest_index + 1].value = std::nan("");
                next_x += dx;
            }
            data[dest_index].value = std::fmin(data[dest_index].value, y);
            data[dest_index + 1].value = std::fmax(data[dest_index + 1].value, y);
        }
        if (dest_index < dest_size - 1)
        {
            data.resize(dest_index + 2);
        }
    }
    return data;
}

struct LineGraphResampler : public AbstractResampler1d
{
    Q_OBJECT

    void _resample_impl(const PyBuffer& x, const PyBuffer& y, const QCPRange new_range,
                        bool new_data) override;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(QList<QVector<QCPGraphData>> data);
#endif

    LineGraphResampler(std::size_t line_cnt);
};

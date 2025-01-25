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

static inline QVector<QCPGraphData> resample(const XYView& view, std::size_t column_index,
                                             std::size_t dest_size)
{
    if (dest_size & 1)
        dest_size++;
    const auto view_size = std::size(view);
    const auto x_0 = view.x(0);
    const auto x_1 = view.x(view_size - 1);
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
        for (auto view_index = 0UL; view_index < view_size; view_index++)
        {
            // @todo: A possible optimization would be to use a kind of iterator
            // that polymorphic view is not cheap
            auto x = view.x(view_index);
            auto y = view.y(view_index, column_index);
            while (x > next_x && dest_index < dest_size - 4)
            {
                dest_index += 2;
                data[dest_index].key = next_x;
                data[dest_index].value = std::nan("");
                data[dest_index + 1].key = next_x + dx / 2.;
                data[dest_index + 1].value = std::nan("");
                next_x = dx * (dest_index + 2) + x_0;
            }

            if (!std::isnan(y)) [[likely]]
            {
                // NaN comparison is always false
                // Either value is NaN or y is smaller than value
                if (!(data[dest_index].value < y))
                    data[dest_index].value = y;
                // Either value is NaN or y is greater than value
                if (!(data[dest_index + 1].value > y))
                    data[dest_index + 1].value = y;
            }
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

    void _resample_impl(const ResamplerData1d& data, const ResamplerPlotInfo& plot_info) override;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(QList<QVector<QCPGraphData>> data);
#endif

    LineGraphResampler(SciQLopPlottableInterface* parent,std::size_t line_cnt);
};

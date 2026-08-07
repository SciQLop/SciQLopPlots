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
#include "SciQLopPlots/Plotables/Resamplers/SciQLopCurveResampler.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include <algorithm>

template <typename X, typename Y>
QVector<QCPCurveData> curve_copy_data(const X* x, const Y* y, std::size_t x_size, const int y_incr)
{
    QVector<QCPCurveData> data(x_size);
    const Y* current_y_it = y;
    for (auto i = 0UL; i < x_size; i++)
    {
        data[i] = QCPCurveData { static_cast<double>(i), static_cast<double>(x[i]),
                                 static_cast<double>(*current_y_it) };
        current_y_it += y_incr;
    }
    return data;
}

void CurveResampler::_resample_impl(const ResamplerData1d& data, const ResamplerPlotInfo& plot_info)
{
    if (data.x.is_valid() && data.x.flat_size() > 0 && data.new_data)
    {
        const auto count = data.x.flat_size();
        // y layout: row-major (n, k) buffers (the natural np.column_stack
        // result) store component i strided at ys + i; anything else keeps the
        // historical contiguous-per-component layout (ys + i*count).
        const bool row_major = data.y.ndim() == 2 && data.y.row_major();
        const auto n_cols = row_major ? data.y.size(1) : 1UL;
        const auto y_incr = row_major ? n_cols : 1UL;
        QList<QVector<QCPCurveData>> curve_data;
        // x/y may be any numeric dtype (set_data validates support); dispatch on
        // both and convert to double. Guarded so an unexpected dtype can't
        // terminate this worker thread.
        // Independent of both dtypes, so computed once rather than inside every
        // dispatch instantiation. A curve built without labels has no components
        // yet and so reports line_count() == 0: emit every column the data holds
        // and let SciQLopCurve size itself from the batch. Otherwise the label
        // list fixes the count, still hard-bounded against the y buffer —
        // line_count() can race ahead of a queued batch (set_line_count runs on
        // the GUI thread), so never trust it alone.
        const auto available = data.y.flat_size() / count;
        const auto lines
            = line_count() > 0 ? std::min(line_count(), available) : available;
        try
        {
            dispatch_dtype(
                data.x.format_code(),
                [&](auto x_tag)
                {
                    dispatch_dtype(
                        data.y.format_code(),
                        [&](auto y_tag)
                        {
                            using X = typename decltype(x_tag)::type;
                            using Y = typename decltype(y_tag)::type;
                            const auto* xs = static_cast<const X*>(data.x.raw_data());
                            const auto* ys = static_cast<const Y*>(data.y.raw_data());
                            for (auto line_index = 0UL; line_index < lines; line_index++)
                                curve_data.emplace_back(
                                    curve_copy_data(xs,
                                                    ys + (row_major ? line_index
                                                                    : line_index * count),
                                                    count, y_incr));
                        });
                });
        }
        catch (const std::invalid_argument&)
        {
            return;
        }
        Q_EMIT setGraphData(curve_data);
    }
}

CurveResampler::CurveResampler(SciQLopPlottableInterface* parent, std::size_t line_cnt)
        : AbstractResampler1d { parent, line_cnt }
{
}

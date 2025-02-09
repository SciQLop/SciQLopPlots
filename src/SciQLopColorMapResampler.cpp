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

#include "SciQLopPlots/Plotables/Resamplers/SciQLopColorMapResampler.hpp"
#include <cpp_utils/containers/algorithms.hpp>

#include "SciQLopPlots/Profiling.hpp"

struct UnprotectQCPColorMapData : public QCPColorMapData
{
    double* data_ptr() { return mData; }

    inline double unsafe_cell(int keyIndex, int valueIndex)
    {
        return mData[valueIndex * mKeySize + keyIndex];
    }

    inline void unsafe_setCell(int keyIndex, int valueIndex, double value)
    {
        mData[valueIndex * mKeySize + keyIndex] = value;
    }
};

inline std::vector<double> _generate_range(double start, double end, std::size_t n,
                                           bool log = false)
{
    std::vector<double> range(n);
    if (log)
    {
        auto step = (std::log10(end) - std::log10(start)) / static_cast<double>(n-1);
        std::generate(std::begin(range), std::end(range),
                      [start, step, i = 0.]() mutable { return start * std::pow(10, i++ * step); });
    }
    else
    {
        auto step = (end - start) / n;
        std::generate(std::begin(range), std::end(range),
                      [start, step, i = 0.]() mutable { return start + i++ * step; });
    }
    return range;
}

template <typename T>
inline void _divide(QCPColorMapData* data, const T& avg_count, const std::size_t n_x,
                    const std::size_t n_y)
{
    PROFILE_HERE;
    auto udata = static_cast<UnprotectQCPColorMapData*>(data);
    for (auto i = 0UL; i < n_x; i++)
    {
        for (auto j = 0UL; j < n_y; j++)
        {
            auto count = static_cast<double>(avg_count[i * n_y + j]);
            if (count != 0)
            {
                udata->unsafe_setCell(i, j, udata->unsafe_cell(i, j) / count);
            }
            else
            {
                udata->unsafe_setCell(i, j, std::nan(""));
            }
        }
    }
}

template <typename T>
inline void _average_value(const XYZView& view, QCPColorMapData* data, T& avg_count,
                           std::size_t x_src_idx, std::size_t x_dest_idx, std::size_t y_src_idx,
                           std::size_t y_dest_idx, std::size_t n_y)
{
    auto udata = static_cast<UnprotectQCPColorMapData*>(data);
    auto z_val = view.z(x_src_idx, y_src_idx);
    udata->unsafe_setCell(x_dest_idx, y_dest_idx,
                          udata->unsafe_cell(x_dest_idx, y_dest_idx) + z_val);
    avg_count[x_dest_idx * n_y + y_dest_idx] += 1;
}

template <typename T, typename U>
inline void _y_loop(const XYZView& view, QCPColorMapData* data, const T& y_axis, U& avg_count,
                    std::size_t x_src_idx, std::size_t x_dest_idx)
{
    PROFILE_HERE;
    auto n_y = std::size(y_axis);
    auto y_dest_idx = 0UL;
    for (auto y_src_idx = 0UL; y_src_idx < view.z_shape().second; y_src_idx++)
    {
        auto y_val = view.y(x_src_idx, y_src_idx);
        while (y_dest_idx < (n_y - 1) && y_val > y_axis[y_dest_idx])
        {
            if (y_src_idx > 0 && y_src_idx != view.z_shape().second - 1)
                _average_value(view, data, avg_count, x_src_idx, x_dest_idx, y_src_idx - 1,
                               y_dest_idx, n_y);
            y_dest_idx++;
        }
        _average_value(view, data, avg_count, x_src_idx, x_dest_idx, y_src_idx, y_dest_idx, n_y);
    }
}

template <typename T, typename U>
inline void _x_loop(const XYZView& view, QCPColorMapData* data, const T& x_axis, const T& y_axis,
                    U& avg_count)
{
    PROFILE_HERE;
    const auto n_x = std::size(x_axis);
    const auto view_size = std::size(view);
    auto x_dest_idx = 0UL;
    auto x_val = view.x(0);
    auto dx = view.x(1) - x_val;
    auto prev_x_val = x_val - dx;
    auto prev_dx = dx;
    for (auto x_src_idx = 0UL; x_src_idx < view_size; x_src_idx++)
    {
        x_val = view.x(x_src_idx);
        dx = x_val - prev_x_val;
        while (x_dest_idx < (n_x - 1) && x_val > x_axis[x_dest_idx])
        {
            if (x_src_idx < std::size(view) - 1)
            {
                auto next_dx = view.x(x_src_idx + 1) - x_val;
                if (((next_dx < (1.5 * dx)) && (dx < 1.5 * prev_dx))
                    || dx == 0.) // data gap criterion (50%) increase in dx
                {
                    _y_loop(view, data, y_axis, avg_count, x_src_idx, x_dest_idx);
                }
            }
            x_dest_idx++;
        }
        _y_loop(view, data, y_axis, avg_count, x_src_idx, x_dest_idx);
        prev_x_val = x_val;
        prev_dx = dx;
    }
}

template <typename T>
inline void _copy_and_average(const XYZView& view, QCPColorMapData* data, const T& x_axis,
                              const T& y_axis)
{
    PROFILE_HERE;
    auto n_x = std::size(x_axis);
    auto n_y = std::size(y_axis);
    std::vector<uint16_t> avg_count(n_x * n_y, 0);
    _x_loop(view, data, x_axis, y_axis, avg_count);
    _divide(data, avg_count, n_x, n_y);
}

auto _generate_axes(const XYZView& view, std::size_t max_x_size, std::size_t max_y_size, bool log)
{
    auto _y_bounds = y_bounds(view);
    auto _x_bounds = std::pair { view.x(0), view.x(std::size(view) - 1) };
    auto n_y = std::min(max_y_size, view.y_shape().second);
    auto n_x = std::min(max_x_size, std::size(view));
    auto x_axis = _generate_range(_x_bounds.first, _x_bounds.second, n_x);
    auto y_axis = _generate_range(_y_bounds.first, _y_bounds.second, n_y, log);
    return std::pair { x_axis, y_axis };
}

void ColormapResampler::_resample_impl(const ResamplerData2d& data,
                                       const ResamplerPlotInfo& plot_info)
{
    PROFILE_HERE_N("ColormapResampler::_resample_impl");
    PROFILE_PASS_VALUE(data.z.flat_size());
    const auto max_x_size = plot_info.plot_size.width();
    const auto max_y_size = plot_info.plot_size.height();
    if (data.x.data() && data.x.flat_size() && max_x_size && max_y_size)
    {
        const auto view = make_view(data, plot_info);
        if (std::size(view) > 10) // less does not make much sense
        {
            auto [x_axis, y_axis]
                = _generate_axes(view, max_x_size, max_y_size, plot_info.y_is_log);
            QCPColorMapData* data = new QCPColorMapData(std::size(x_axis), std::size(y_axis),
                                                        { x_axis.front(), x_axis.back() },
                                                        { y_axis.front(), y_axis.back() });
            _copy_and_average(view, data, x_axis, y_axis);
            data->recalculateDataBounds();
            Q_EMIT setGraphData(data);
        }
    }
}

ColormapResampler::ColormapResampler(SciQLopPlottableInterface* parent, bool y_scale_is_log)
        : AbstractResampler2d { parent }
{
    connect(parent->y_axis(), &SciQLopPlotAxisInterface::log_changed, this,
            [this](bool log) { set_y_scale_log(log); });
    set_y_scale_log(y_scale_is_log);
}

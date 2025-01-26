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

#include "SciQLopPlots/Plotables/Resamplers/SciQLopLineGraphResampler.hpp"
#include "SciQLopPlots/Profiling.hpp"

LineGraphResampler::LineGraphResampler(SciQLopPlottableInterface* parent, std::size_t line_cnt)
        : AbstractResampler1d { parent, line_cnt }
{
    connect(parent->x_axis(), &SciQLopPlotAxisInterface::log_changed, this,
            [this](bool log) { this->set_x_scale_log(log); });
    this->set_x_scale_log(parent->x_axis()->log());
}

void LineGraphResampler::_resample_impl(const ResamplerData1d& data,
                                        const ResamplerPlotInfo& plot_info)
{
    PROFILE_HERE_N("LineGraphResampler::_resample_impl");
    PROFILE_PASS_VALUE(data.x.flat_size());
    // 4x the plot width is a reasonable maximum size for the data,
    // with anti-aliasing we have to provide a little points than pixels
    const auto max_x_size = static_cast<std::size_t>(plot_info.plot_size.width() * 4);
    if (data.x.data() && data.x.flat_size() && max_x_size)
    {
        const auto view = make_view(data, plot_info);
        if (std::size(view))
        {
            QList<QVector<QCPGraphData>> data;
            for (auto line_index = 0UL; line_index < line_count(); line_index++)
            {
                if ((std::size(view) > max_x_size) && !plot_info.x_is_log)
                {
                    data.emplace_back(::resample(view, line_index, max_x_size));
                }
                else
                {
                    data.emplace_back(::copy_data(view, line_index));
                }
            }
            Q_EMIT setGraphData(data);
        }
    }
}

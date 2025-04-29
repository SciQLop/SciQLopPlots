/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp"

SciQLopNDProjectionCurves::SciQLopNDProjectionCurves(SciQLopPlotInterface* parent,
                                                     QList<SciQLopPlot*>& plots,
                                                     const QStringList& labels, QVariantMap metaData)
        : SciQLopGraphInterface("Projection",metaData, parent)
{
    if (plots.size() != labels.size())
    {
        DEBUG_MESSAGE("Invalid input size");
        return;
    }
    for (int i = 0; i < plots.size(); ++i)
    {
        auto curve = qobject_cast<SciQLopCurve*>(
            plots[i]->parametric_curve(PyBuffer(), PyBuffer(), { labels[i] }));
        m_curves.append(curve);
    }
}

SciQLopNDProjectionCurvesFunction::SciQLopNDProjectionCurvesFunction(SciQLopPlotInterface* parent,
                                                                     QList<SciQLopPlot*>& plots,
                                                                     GetDataPyCallable&& callable,
                                                                     const QStringList& labels, QVariantMap metaData)
        : SciQLopNDProjectionCurves { parent, plots, labels, metaData }
        , SciQLopFunctionGraph(std::move(callable),this, 4)
{
    /*m_pipeline = new SimplePyCallablePipeline(std::move(callable), this);
    connect(m_pipeline, &SimplePyCallablePipeline::new_data_nd, this,
            &SciQLopNDProjectionCurvesFunction::_set_data);
    connect(this, &SciQLopLineGraph::range_changed, m_pipeline,
            &SimplePyCallablePipeline::set_range);*/
}

void SciQLopNDProjectionCurves::set_selected(bool selected) noexcept
{
    for (auto curve : std::as_const(m_curves))
    {
        curve->set_selected(selected);
    }
}

bool SciQLopNDProjectionCurves::selected() const noexcept
{
    if (!m_curves.isEmpty())
    {
        return m_curves[0]->selected();
    }
    return false;
}

void SciQLopNDProjectionCurves::set_data(const QList<PyBuffer>& data)
{
    const auto curves_count = m_curves.size();

    if (data.size() == curves_count + 1)
    {
        auto data_without_time = data.sliced(1);
        // Expected sequence for 3 curves:
        // Y(x)
        // Z(y)
        // Z(x)
        for (decltype(data.size()) i = 0; i < curves_count; ++i)
        {
            m_curves[i]->set_data(data_without_time[i % (curves_count - 1)],
                                  data_without_time[std::min((i + 1), curves_count - 1)]);
        }
    }
    else if (data.size() == 2 * curves_count)
    {
        for (decltype(data.size()) i = 0; i < curves_count; ++i)
        {
            m_curves[i]->set_data(data[2 * i], data[2 * i + 1]);
        }
    }
    else
    {
        DEBUG_MESSAGE("Invalid data size");
    }
}

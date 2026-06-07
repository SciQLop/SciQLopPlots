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
#include "SciQLopPlots/Python/DtypeDispatch.hpp"

#include <algorithm>

namespace
{
// Copy any numeric PyBuffer to QVector<double>, converting per its dtype.
// SciQLopPyBuffer::data() is double-only and throws (→ std::terminate) on
// float32/int buffers, which Speasy routinely produces.
QVector<double> to_double_vector(const SciQLopPyBuffer& buffer)
{
    QVector<double> out(static_cast<int>(buffer.flat_size()));
    dispatch_dtype(buffer.format_code(),
                   [&](auto tag)
                   {
                       using V = typename decltype(tag)::type;
                       const auto* src = static_cast<const V*>(buffer.raw_data());
                       std::transform(src, src + buffer.flat_size(), out.data(),
                                      [](V v) { return static_cast<double>(v); });
                   });
    return out;
}
} // namespace

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
            plots[i]->parametric_curve(SciQLopPyBuffer(), SciQLopPyBuffer(), { labels[i] }));
        if (curve)
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

void SciQLopNDProjectionCurves::set_data(const QList<SciQLopPyBuffer>& data)
{
    const auto curves_count = m_curves.size();

    if (curves_count < 2)
    {
        DEBUG_MESSAGE("Need at least 2 curves for projection");
        return;
    }

    if (data.size() == curves_count + 1)
    {
        auto data_without_time = data.sliced(1);
        for (decltype(data.size()) i = 0; i < curves_count; ++i)
        {
            m_curves[i]->set_data(data_without_time[i % curves_count],
                                  data_without_time[(i + 1) % curves_count]);
        }
        const auto& time_buf = data[0];
        QVector<double> times = to_double_vector(time_buf);
        for (auto* curve : std::as_const(m_curves))
            curve->set_time_values(times);
    }
    else if (data.size() == 3 * curves_count)
    {
        for (decltype(data.size()) i = 0; i < curves_count; ++i)
        {
            m_curves[i]->set_data(data[3 * i], data[3 * i + 1]);
            const auto& scalar_buf = data[3 * i + 2];
            QVector<double> scalars = to_double_vector(scalar_buf);
            m_curves[i]->set_color_values(scalars);
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

void SciQLopNDProjectionCurves::set_colors(const QList<QColor>& colors)
{
    for (int i = 0; i < m_curves.size() && i < colors.size(); ++i)
        m_curves[i]->set_colors({ colors[i] });
}

void SciQLopNDProjectionCurves::set_time_color_enabled(bool enabled)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_time_color_enabled(enabled);
}

bool SciQLopNDProjectionCurves::time_color_enabled() const
{
    return !m_curves.isEmpty() && m_curves.first()->time_color_enabled();
}

void SciQLopNDProjectionCurves::set_time_color_gradient(const QColor& start, const QColor& end)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_time_color_gradient(start, end);
}

QList<std::optional<QPointF>> SciQLopNDProjectionCurves::positions_at_time(double t) const
{
    QList<std::optional<QPointF>> result;
    for (auto* curve : std::as_const(m_curves))
        result.append(curve->position_at_time(t));
    return result;
}

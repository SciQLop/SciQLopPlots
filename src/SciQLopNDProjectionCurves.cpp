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
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

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
    connect_pipeline_colored_data_to_graph(m_pipeline, this);
    // Fetch for the range the plot already has, like the other function graphs:
    // the panel applies its time range before the graph exists, so waiting for the
    // next change would leave the graph empty until someone pans.
    if (auto* time_axis = parent->time_axis())
    {
        const auto range = time_axis->range();
        if (range.start() != range.stop())
            this->set_range(range);
    }
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
        QVector<double> times = to_double_vector<QVector<double>>(time_buf);
        for (auto* curve : std::as_const(m_curves))
            curve->set_time_values(times);
    }
    else if (data.size() == 3 * curves_count)
    {
        for (decltype(data.size()) i = 0; i < curves_count; ++i)
        {
            m_curves[i]->set_data(data[3 * i], data[3 * i + 1]);
            const auto& scalar_buf = data[3 * i + 2];
            QVector<double> scalars = to_double_vector<QVector<double>>(scalar_buf);
            m_curves[i]->set_color_values(scalars);
            m_curves[i]->set_time_color_enabled(!scalars.isEmpty());
        }
        _update_color_scale();
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

void SciQLopNDProjectionCurves::set_data_and_color(const QList<SciQLopPyBuffer>& data,
                                                   const SciQLopPyBuffer& color)
{
    if (data.size() != m_curves.size() + 1)
        throw std::invalid_argument("Projection: a coloured batch needs [t, d0..d"
                                    + std::to_string(m_curves.size() - 1) + "], got "
                                    + std::to_string(data.size()) + " buffers");
    const std::size_t samples = data[0].is_valid() ? data[0].flat_size() : 0;
    const std::size_t count = color.is_valid() ? color.flat_size() : 0;
    if (count > 0 && count != samples)
        throw std::invalid_argument("Projection: expected one colour value per time sample ("
                                    + std::to_string(samples) + "), got "
                                    + std::to_string(count));
    set_data(data);
    const auto values = count > 0 ? to_double_vector<QVector<double>>(color) : QVector<double> {};
    for (auto* curve : std::as_const(m_curves))
    {
        curve->set_color_values(values);
        curve->set_time_color_enabled(!values.isEmpty());
    }
    _update_color_scale();
}

void SciQLopNDProjectionCurves::set_colors(const QList<QColor>& colors)
{
    for (int i = 0; i < m_curves.size() && i < colors.size(); ++i)
        m_curves[i]->set_colors({ colors[i] });
}

QList<QColor> SciQLopNDProjectionCurves::colors() const noexcept
{
    QList<QColor> colors;
    for (auto* curve : m_curves)
        colors.append(curve->colors().value(0));
    return colors;
}

void SciQLopNDProjectionCurves::set_color_data(SciQLopPyBuffer values, ::ColorGradient gradient)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_color_data(values, gradient);
    _update_color_scale();
    if (values.is_valid() && values.flat_size() > 0)
        _set_scale_gradient(gradient);
}

void SciQLopNDProjectionCurves::set_color_gradient(::ColorGradient gradient)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_color_gradient(gradient);
    _set_scale_gradient(gradient);
}

SciQLopNDProjectionCurves::~SciQLopNDProjectionCurves()
{
    // Still listed among the plot's plottables here, so let it look again once we are gone.
    if (auto* plot = qobject_cast<SciQLopNDProjectionPlot*>(parent()))
        QMetaObject::invokeMethod(plot, [plot] { plot->update_color_scale(); },
                                  Qt::QueuedConnection);
}

void SciQLopNDProjectionCurves::_update_color_scale()
{
    if (auto* plot = qobject_cast<SciQLopNDProjectionPlot*>(parent()))
        plot->update_color_scale();
}

void SciQLopNDProjectionCurves::_set_scale_gradient(::ColorGradient gradient)
{
    // Not request_z_gradient: projection panes never host a colormap, nothing to keep away.
    if (auto* plot = qobject_cast<SciQLopNDProjectionPlot*>(parent()))
        plot->set_z_gradient(gradient);
}

void SciQLopNDProjectionCurves::attach_color_scale(QCPColorScale* scale)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_color_scale(scale);
}

bool SciQLopNDProjectionCurves::has_color_values() const
{
    return std::any_of(m_curves.begin(), m_curves.end(),
                       [](auto* curve) { return curve->has_color_values(); });
}

std::optional<std::pair<double, double>> SciQLopNDProjectionCurves::color_range(bool log) const
{
    std::optional<std::pair<double, double>> range;
    for (auto* curve : m_curves)
        if (const auto r = curve->color_range(log))
            range = range ? std::pair { std::min(range->first, r->first),
                                        std::max(range->second, r->second) }
                          : *r;
    return range;
}

void SciQLopNDProjectionCurves::set_line_width(qreal width)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_line_width(width);
}

qreal SciQLopNDProjectionCurves::line_width() const
{
    return m_curves.isEmpty() ? 1.0 : m_curves.first()->line_width();
}

void SciQLopNDProjectionCurves::set_visible(bool visible) noexcept
{
    const bool changed = !m_curves.isEmpty() && this->visible() != visible;
    for (auto* curve : std::as_const(m_curves))
        curve->set_visible(visible);
    _update_color_scale();
    if (changed)
        Q_EMIT visible_changed(visible);
}

bool SciQLopNDProjectionCurves::visible() const noexcept
{
    return std::any_of(m_curves.begin(), m_curves.end(),
                       [](auto* curve) { return curve->visible(); });
}

QList<SciQLopGraphComponentInterface*> SciQLopNDProjectionCurves::components() const noexcept
{
    QList<SciQLopGraphComponentInterface*> all;
    for (auto* curve : m_curves)
        all.append(curve->components());
    return all;
}

SciQLopGraphComponentInterface* SciQLopNDProjectionCurves::component(int index) const noexcept
{
    return components().value(index, nullptr);
}

SciQLopGraphComponentInterface* SciQLopNDProjectionCurves::component(const QString& name) const noexcept
{
    for (auto* component : components())
        if (component->name() == name)
            return component;
    return nullptr;
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
    if (auto* plot = qobject_cast<SciQLopNDProjectionPlot*>(parent()))
        plot->set_z_gradient_colors(start, end);
}

QList<QVariant> SciQLopNDProjectionCurves::positions_at_time(double t) const
{
    QList<QVariant> result;
    for (auto* curve : std::as_const(m_curves))
        result.append(curve->position_at_time(t));
    return result;
}

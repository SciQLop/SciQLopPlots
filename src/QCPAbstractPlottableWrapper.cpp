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

#include "SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"

#include <algorithm>

namespace
{
SciQLopPlot* owning_plot(QObject* graph_parent)
{
    auto* impl = qobject_cast<_impl::SciQLopPlot*>(graph_parent);
    return impl ? qobject_cast<SciQLopPlot*>(impl->parent()) : nullptr;
}
}

void SQPQCPAbstractPlottableWrapper::notify_color_scale(std::optional<::ColorGradient> gradient)
{
    auto* plot = owning_plot(parent());
    if (!plot)
        return;
    plot->update_curve_color_scale();
    if (gradient)
        plot->request_z_gradient(*gradient);
}

void SQPQCPAbstractPlottableWrapper::notify_color_scale_later()
{
    if (auto* plot = owning_plot(parent()))
        QMetaObject::invokeMethod(plot, [plot] { plot->update_curve_color_scale(); },
                                  Qt::QueuedConnection);
}

void SQPQCPAbstractPlottableWrapper::_register_component(SciQLopGraphComponent* component)
{
    m_components.append(component);
    if (component->plottable())
        component->plottable()->setWantsLegend(in_legend());
    connect(component, &SciQLopGraphComponent::replot, this,
            &SQPQCPAbstractPlottableWrapper::replot);
    // Connected before the inspector model sees the component, so this runs
    // before the model removes its row and the tree re-selects this graph.
    connect(component, &QObject::destroyed, this,
            &SQPQCPAbstractPlottableWrapper::_forget_destroyed_components);
    Q_EMIT this->component_list_changed();
}

void SQPQCPAbstractPlottableWrapper::_forget_destroyed_components()
{
    // destroyed() fires after Qt has nulled the QPointer to the dying component.
    if (m_components.removeIf([](const auto& c) { return c.isNull(); }) > 0)
        Q_EMIT this->component_list_changed();
}

void SQPQCPAbstractPlottableWrapper::set_labels(const QStringList& labels)
{
    if (std::size(labels) == std::size(m_components))
    {
        for (decltype(std::size(m_components)) i = 0UL; i < std::size(m_components); ++i)
        {
            m_components[i]->set_name(labels[i]);
        }
        Q_EMIT labels_changed(labels);
    }
    else
    {
        throw std::runtime_error("Invalid number of labels");
    }
}

void SQPQCPAbstractPlottableWrapper::set_colors(const QList<QColor>& colors)
{
    if (std::size(colors) == std::size(m_components))
    {
        for (decltype(std::size(m_components)) i = 0; i < std::size(m_components); ++i)
        {
            m_components[i]->set_color(colors[i]);
        }
        Q_EMIT colors_changed(colors);
    }
    else
    {
        throw std::runtime_error("Invalid number of colors");
    }
}

QStringList SQPQCPAbstractPlottableWrapper::labels() const noexcept
{
    QStringList labels;
    for (const auto& plottable : m_components)
    {
        labels.append(plottable->name());
    }
    return labels;
}

QList<QColor> SQPQCPAbstractPlottableWrapper::colors() const noexcept
{
    QList<QColor> colors;
    for (const auto& plottable : m_components)
    {
        colors.append(plottable->color());
    }
    return colors;
}

// The components are copied: a slot of one may delete another.
void SQPQCPAbstractPlottableWrapper::_set_visible(bool visible)
{
    const auto components = m_components;
    for (const auto& component : components)
        if (component)
            component->set_visible(visible);
}

bool SQPQCPAbstractPlottableWrapper::_visible() const
{
    return std::ranges::any_of(m_components, [](const auto& c) { return c && c->visible(); });
}

void SQPQCPAbstractPlottableWrapper::_set_selected(bool selected)
{
    const auto components = m_components;
    for (const auto& component : components)
        if (component)
            component->set_selected(selected);
}

bool SQPQCPAbstractPlottableWrapper::_selected() const
{
    return std::ranges::any_of(m_components, [](const auto& c) { return c && c->selected(); });
}

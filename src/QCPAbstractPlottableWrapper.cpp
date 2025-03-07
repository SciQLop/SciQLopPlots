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

void SQPQCPAbstractPlottableWrapper::_register_component(SciQLopGraphComponent* component)
{
    m_components.append(component);
    connect(component, &SciQLopGraphComponent::replot, this,
            &SQPQCPAbstractPlottableWrapper::replot);
    Q_EMIT this->component_list_changed();
}

void SQPQCPAbstractPlottableWrapper::set_visible(bool visible) noexcept
{
    for (auto plottable : m_components)
    {
        plottable->set_visible(visible);
    }
    Q_EMIT visible_changed(visible);
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

bool SQPQCPAbstractPlottableWrapper::visible() const noexcept
{
    // True if at least one plottable is visible
    for (const auto& plottable : m_components)
    {
        if (plottable->visible())
            return true;
    }
    return false;
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

void SQPQCPAbstractPlottableWrapper::set_selected(bool selected) noexcept
{
    bool changed = false;
    /*for (auto plottable : m_components)
    {
        if (plottable->selected() != selected)
        {
            plottable->set_selected(selected);
            changed = true;
        }
    }*/
    if (changed)
        Q_EMIT selection_changed(selected);
}

bool SQPQCPAbstractPlottableWrapper::selected() const noexcept
{
    // True if at least one plottable is selected
    for (const auto& plottable : m_components)
    {
        if (plottable->selected())
            return true;
    }
    return false;
}

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
#include "SciQLopPlots/Plotables/SciQLopGraphComponent.hpp"

SciQLopGraphComponent::SciQLopGraphComponent(QCPAbstractPlottable* plottable, QObject* parent)
        : SciQLopGraphComponentInterface(parent), m_plottable(plottable)
{
    if (m_plottable)
    {
        this->setObjectName(m_plottable->name());
        if (auto legend_item = _legend_item(); legend_item)
        {
            connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                    &SciQLopGraphComponent::set_selected);
        }
        if (auto graph = qobject_cast<QCPGraph*>(m_plottable); graph)
        {
            // Disable adaptive sampling because it does not play well with SciQLopPlots resamplers
            graph->setAdaptiveSampling(false);
        }
        connect(plottable, QOverload<bool>::of(&QCPAbstractPlottable::selectionChanged), this,
                &SciQLopGraphComponent::set_selected);
    }
}

SciQLopGraphComponent::~SciQLopGraphComponent()
{
    if (m_plottable)
    {
        if (QCustomPlot* plot = dynamic_cast<QCustomPlot*>(m_plottable->parentPlot());
            plot && plot->hasPlottable(m_plottable.data()))
        {
            plot->removePlottable(m_plottable.data());
        }
    }
    set_selected(false);
}

void SciQLopGraphComponent::set_pen(const QPen& pen) noexcept
{
    if (m_plottable)
    {
        m_plottable->setPen(pen);
        QPen selectionPen = pen;
        selectionPen.setColor(Qt::darkGray);
        selectionPen.setWidthF(pen.widthF() + 3);
        m_plottable->selectionDecorator()->setPen(selectionPen);
        emit replot();
    }
}

void SciQLopGraphComponent::set_line_width(const qreal width) noexcept
{
    if (m_plottable)
    {
        auto pen = m_plottable->pen();
        pen.setWidthF(width);
        m_plottable->setPen(pen);
        emit replot();
    }
}

void SciQLopGraphComponent::set_visible(bool visible) noexcept
{
    if (m_plottable)
    {
        m_plottable->setVisible(visible);
        emit visible_changed(visible);
        emit replot();
    }
}

void SciQLopGraphComponent::set_name(const QString& name) noexcept
{
    this->setObjectName(name);
    if (m_plottable && m_plottable->name() != name)
    {
        m_plottable->setName(name);
        emit name_changed(name);
    }
}

void SciQLopGraphComponent::set_selected(bool selected) noexcept
{
    if (m_plottable)
    {
        if (m_selected != selected)
        {
            m_selected = selected;
            std::visit(visitor { [selected](auto any)
                                 {
                                     if (selected)
                                     {
                                         any->setSelection(
                                             QCPDataSelection(any->data()->dataRange()));
                                     }
                                     else
                                     {
                                         any->setSelection(QCPDataSelection());
                                     }
                                 },
                                 [](std::monostate) {} },
                       to_variant());
            emit selection_changed(selected);
            emit replot();
        }
        if (auto legend_item = _legend_item(); legend_item && legend_item->selected() != selected)
        {
            legend_item->setSelected(selected);
            emit replot();
        }
    }
}

QColor SciQLopGraphComponent::color() const noexcept
{
    if (m_plottable)
    {
        return m_plottable->pen().color();
    }
    return QColor();
}

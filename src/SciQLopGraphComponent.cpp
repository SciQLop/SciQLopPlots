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

SciQLopGraphComponent::SciQLopGraphComponent(QCPMultiGraph* multiGraph, int componentIndex,
                                             QObject* parent)
        : SciQLopGraphComponentInterface(parent)
        , m_plottable(multiGraph)
        , m_componentIndex(componentIndex)
{
    connect(multiGraph, QOverload<bool>::of(&QCPAbstractPlottable::selectionChanged), this,
            [this, multiGraph]() {
                bool componentSelected =
                    !multiGraph->componentSelection(m_componentIndex).isEmpty();
                if (m_selected != componentSelected)
                    set_selected(componentSelected);
            });
    if (auto* gi = _group_legend_item(); gi)
    {
        connect(gi, &QCPGroupLegendItem::componentClicked, this,
                [this](int clickedComponent) {
                    set_selected(clickedComponent == m_componentIndex);
                });
    }
}

SciQLopGraphComponent::~SciQLopGraphComponent()
{
    if (m_componentIndex >= 0)
    {
        set_selected(false);
        return;
    }
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
        if (m_componentIndex >= 0)
        {
            auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
            if (mg)
            {
                mg->component(m_componentIndex).pen = pen;
                QPen selPen = pen;
                selPen.setColor(Qt::darkGray);
                selPen.setWidthF(pen.widthF() + 3);
                mg->component(m_componentIndex).selectedPen = selPen;
            }
            emit replot();
            return;
        }
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
        if (m_componentIndex >= 0)
        {
            auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
            if (mg)
                mg->component(m_componentIndex).pen.setWidthF(width);
            emit replot();
            return;
        }
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
        if (m_componentIndex >= 0)
        {
            auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
            if (mg)
                mg->component(m_componentIndex).visible = visible;
            emit visible_changed(visible);
            emit replot();
            return;
        }
        m_plottable->setVisible(visible);
        emit visible_changed(visible);
        emit replot();
    }
}

void SciQLopGraphComponent::set_name(const QString& name) noexcept
{
    this->setObjectName(name);
    if (m_plottable)
    {
        if (m_componentIndex >= 0)
        {
            auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
            if (mg && mg->component(m_componentIndex).name != name)
            {
                mg->component(m_componentIndex).name = name;
                emit name_changed(name);
            }
            return;
        }
        if (m_plottable->name() != name)
        {
            m_plottable->setName(name);
            emit name_changed(name);
        }
    }
}

void SciQLopGraphComponent::set_selected(bool selected) noexcept
{
    if (m_plottable)
    {
        if (m_selected != selected)
        {
            m_selected = selected;
            if (m_componentIndex >= 0)
            {
                auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
                if (mg)
                {
                    if (selected)
                        mg->setComponentSelection(
                            m_componentIndex,
                            QCPDataSelection(QCPDataRange(0, mg->dataCount())));
                    else
                        mg->setComponentSelection(m_componentIndex, QCPDataSelection());
                }
            }
            else
            {
                std::visit(visitor { [selected](QCPGraph* any)
                                     {
                                         if (selected)
                                             any->setSelection(
                                                 QCPDataSelection(any->data()->dataRange()));
                                         else
                                             any->setSelection(QCPDataSelection());
                                     },
                                     [selected](QCPGraph2* any)
                                     {
                                         if (selected)
                                             any->setSelection(
                                                 QCPDataSelection(QCPDataRange(0, any->dataCount())));
                                         else
                                             any->setSelection(QCPDataSelection());
                                     },
                                     [selected](QCPCurve* any)
                                     {
                                         if (selected)
                                             any->setSelection(
                                                 QCPDataSelection(any->data()->dataRange()));
                                         else
                                             any->setSelection(QCPDataSelection());
                                     },
                                     [](MultiGraphRef) {},
                                     [](std::monostate) {} },
                           to_variant());
            }
            emit selection_changed(selected);
            emit replot();
        }
        if (m_componentIndex >= 0)
        {
            if (auto* gi = _group_legend_item(); gi)
            {
                if (selected && gi->selectedComponent() != m_componentIndex)
                {
                    gi->setExpanded(true);
                    gi->setSelectedComponent(m_componentIndex);
                    gi->setSelected(true);
                    emit replot();
                }
                else if (!selected && gi->selected()
                         && gi->selectedComponent() == m_componentIndex)
                {
                    gi->setSelected(false);
                    emit replot();
                }
            }
        }
        else if (auto legend_item = _legend_item(); legend_item && legend_item->selected() != selected)
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
        if (m_componentIndex >= 0)
        {
            auto mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
            if (mg)
                return mg->component(m_componentIndex).pen.color();
            return QColor();
        }
        return m_plottable->pen().color();
    }
    return QColor();
}

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
#include "SciQLopPlots/Python/Validation.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include <QMouseEvent>
#include <layoutelements/layoutelement-legend.h>
#include <layoutelements/layoutelement-legend-group.h>

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
    _connect_legend_visibility();
}

SciQLopGraphComponent::SciQLopGraphComponent(QCPMultiGraph* multiGraph, int componentIndex,
                                             QObject* parent)
        : SciQLopGraphComponentInterface(parent)
        , m_plottable(multiGraph)
        , m_componentIndex(componentIndex)
{
    // The base compares set_name against objectName, so it must start as the name NeoQCP draws.
    setObjectName(multiGraph->component(componentIndex).name);
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
    _connect_legend_visibility();
}

// The hook, not set_selected: a component going away is no selection change to its graph,
// the same as one a shrinking set_data drops (it re-seeds silently). Only a selected one has a
// frame and a selection to clear.
SciQLopGraphComponent::~SciQLopGraphComponent()
{
    if (m_componentIndex >= 0)
    {
        if (m_selected)
            _set_selected(false);
        return;
    }
    if (m_plottable)
    {
        if (QCustomPlot* plot = dynamic_cast<QCustomPlot*>(m_plottable->parentPlot());
            plot && plot->hasPlottable(m_plottable.data()))
        {
            (void)plot->removePlottable(m_plottable.data());
        }
    }
    if (m_selected)
        _set_selected(false);
}

void SciQLopGraphComponent::set_pen(const QPen& pen)
{
    SCIQLOP_ON_OWNER_THREAD(set_pen(pen));
    const QColor before = _color();
    _apply_pen(pen);
    _notify_color_since(before);
}

void SciQLopGraphComponent::_apply_pen(const QPen& pen)
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

void SciQLopGraphComponent::set_line_width(const qreal width)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_width(width));
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

void SciQLopGraphComponent::_set_visible(bool visible)
{
    if (!m_plottable)
        return;
    if (m_componentIndex < 0)
        m_plottable->setVisible(visible);
    else if (auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data()))
        mg->setComponentVisible(m_componentIndex, visible);
    emit replot();
}

bool SciQLopGraphComponent::_visible() const
{
    if (!m_plottable)
        return false;
    if (m_componentIndex < 0)
        return m_plottable->visible();
    auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
    return mg && mg->component(m_componentIndex).visible;
}

void SciQLopGraphComponent::_named(const QString& name)
{
    if (!m_plottable)
        return;
    if (m_componentIndex < 0)
        m_plottable->setName(name);
    else if (auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data()))
        mg->component(m_componentIndex).name = name;
}

void SciQLopGraphComponent::_set_color(const QColor& color)
{
    if (!m_plottable)
        return;
    auto line = pen();
    line.setColor(color);
    set_pen(line);
    auto marker = marker_pen();
    if (marker.style() == Qt::NoPen)
        marker = QPen(color);
    else
        marker.setColor(color);
    set_marker_pen(marker);
}

void SciQLopGraphComponent::_set_selected(bool selected)
{
    if (!m_plottable)
        return;
    // Stored first: NeoQCP's selectionChanged comes back to set_selected, which then sees no change.
    m_selected = selected;
    _select_in_plottable(selected);
    _select_in_legend(selected);
    emit replot();
}

void SciQLopGraphComponent::_select_in_plottable(bool selected)
{
    if (m_componentIndex >= 0)
    {
        if (auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data()))
            mg->setComponentSelection(m_componentIndex,
                                      selected ? QCPDataSelection(QCPDataRange(0, mg->dataCount()))
                                               : QCPDataSelection());
        return;
    }
    std::visit(visitor { [selected](auto* any)
                         {
                             any->setSelection(selected
                                                   ? QCPDataSelection(any->data()->dataRange())
                                                   : QCPDataSelection());
                         },
                         [](MultiGraphRef) {}, [](std::monostate) {} },
               to_variant());
}

namespace
{
QList<int> selected_components(const QCPMultiGraph& graph)
{
    QList<int> selected;
    for (int i = 0; i < graph.componentCount(); ++i)
        if (!graph.componentSelection(i).isEmpty())
            selected.append(i);
    return selected;
}

// NeoQCP frames one row of a group, or the whole group (row -1): several selected rows get the
// whole group.
void frame_selected_components(QCPGroupLegendItem* group)
{
    const auto selected = selected_components(*group->multiGraph());
    if (selected.isEmpty())
    {
        group->setSelected(false);
        return;
    }
    group->setSelectedComponent(selected.size() == 1 ? selected.first() : -1);
    group->setSelected(true);
}
}

void SciQLopGraphComponent::_select_in_legend(bool selected)
{
    if (m_componentIndex < 0)
    {
        if (auto* item = _legend_item(); item && item->selected() != selected)
            item->setSelected(selected);
        return;
    }
    if (auto* group = _group_legend_item())
    {
        if (selected)
            group->setExpanded(true);
        frame_selected_components(group);
    }
}

QColor SciQLopGraphComponent::_color() const
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

void SciQLopGraphComponent::_connect_legend_visibility()
{
    if (auto* plot = _plot())
        connect(plot, &QCustomPlot::legendDoubleClick, this,
                &SciQLopGraphComponent::_on_legend_double_clicked);
    connect(this, &SciQLopGraphComponentInterface::visible_changed, this,
            &SciQLopGraphComponent::_apply_legend_visibility_style);
}

void SciQLopGraphComponent::_on_legend_double_clicked(QCPLegend* legend,
                                                      QCPAbstractLegendItem* item,
                                                      QMouseEvent* event)
{
    Q_UNUSED(legend);
    Q_UNUSED(event);
    bool mine = false;
    if (m_componentIndex >= 0)
    {
        // selectedComponent() is already set by the time legendDoubleClick fires:
        // Qt delivers the press/release selection event before the double-click event.
        if (auto* gi = dynamic_cast<QCPGroupLegendItem*>(item);
            gi != nullptr && gi->multiGraph() == qobject_cast<QCPMultiGraph*>(m_plottable.data())
            && gi->selectedComponent() == m_componentIndex)
            mine = true;
    }
    else if (auto* li = dynamic_cast<QCPPlottableLegendItem*>(item);
             li != nullptr && li == _legend_item())
    {
        mine = true;
    }
    if (mine)
        set_visible(!visible());
}

void SciQLopGraphComponent::_apply_legend_visibility_style(bool visible)
{
    // Only single-plottable components have a per-entry legend item to dim;
    // multi-component group items have no per-row text colour (matches prior
    // behaviour — the curve simply disappears).
    if (auto* li = _legend_item(); li != nullptr)
    {
        // Restore the legend's themed text colour when visible; dim via alpha
        // when hidden so it reads correctly on both light and dark themes
        // (never hard-code black/gray — black is invisible on a dark legend).
        QColor c = li->parentLegend() ? li->parentLegend()->textColor() : li->textColor();
        if (!visible)
            c.setAlpha(90);
        li->setTextColor(c);
        li->setSelectedTextColor(c);
    }
}

std::optional<MultiGraphRef> SciQLopGraphComponent::multi_ref() const
{
    const auto variant = to_variant();
    if (const auto* ref = std::get_if<MultiGraphRef>(&variant); ref && ref->graph)
        return *ref;
    return std::nullopt;
}

int SciQLopGraphComponent::sibling_index(SciQLopGraphComponentInterface* other) const
{
    const auto* sibling = qobject_cast<SciQLopGraphComponent*>(other);
    const auto mine = multi_ref();
    const auto theirs = sibling ? sibling->multi_ref() : std::nullopt;
    if (!mine || !theirs || mine->graph != theirs->graph)
        throw std::invalid_argument("the other component must belong to the same graph");
    return theirs->componentIndex;
}

void SciQLopGraphComponent::set_fill_to_value(double value)
{
    sqp::validation::validate_finite(value, "set_fill_to_value");
    SCIQLOP_ON_OWNER_THREAD(set_fill_to_value(value));
    const auto ref = multi_ref();
    if (!ref)
    {
        WARN_UNSUPPORTED_FUNCTIONALITY;
        return;
    }
    ref->graph->setComponentFill(ref->componentIndex, value);
    Q_EMIT fill_changed();
    Q_EMIT replot();
}

void SciQLopGraphComponent::set_fill_between(SciQLopGraphComponentInterface* other)
{
    SCIQLOP_ON_OWNER_THREAD_WITH(other, set_fill_between(other));
    const auto ref = multi_ref();
    if (!ref)
    {
        WARN_UNSUPPORTED_FUNCTIONALITY;
        return;
    }
    const int target = sibling_index(other);
    if (!ref->graph->setComponentFillBetween(ref->componentIndex, target))
        throw std::invalid_argument("a component cannot be filled against itself");
    Q_EMIT fill_changed();
    Q_EMIT replot();
}

void SciQLopGraphComponent::clear_fill()
{
    SCIQLOP_ON_OWNER_THREAD(clear_fill());
    const auto ref = multi_ref();
    if (!ref)
    {
        WARN_UNSUPPORTED_FUNCTIONALITY;
        return;
    }
    ref->graph->clearComponentFill(ref->componentIndex);
    Q_EMIT fill_changed();
    Q_EMIT replot();
}

void SciQLopGraphComponent::set_fill_color(const QColor& color)
{
    SCIQLOP_ON_OWNER_THREAD(set_fill_color(color));
    const auto ref = multi_ref();
    if (!ref)
    {
        WARN_UNSUPPORTED_FUNCTIONALITY;
        return;
    }
    if (!has_fill())
        throw std::invalid_argument("this component has no fill");
    ref->graph->setComponentFillBrush(ref->componentIndex, QBrush(color));
    Q_EMIT fill_changed();
    Q_EMIT replot();
}

bool SciQLopGraphComponent::has_fill() const
{
    const auto ref = multi_ref();
    return ref && ref->graph->componentFill(ref->componentIndex).has_value();
}

QColor SciQLopGraphComponent::fill_color() const
{
    const auto ref = multi_ref();
    const auto fill = ref ? ref->graph->componentFill(ref->componentIndex) : std::nullopt;
    return fill ? ref->graph->effectiveFillBrush(*fill).color() : QColor();
}

void SciQLopGraphComponent::set_error_bars(SciQLopGraphComponentInterface* lower,
                                           SciQLopGraphComponentInterface* upper,
                                           double cap_width)
{
    if (!(cap_width >= 0))
        throw std::invalid_argument("set_error_bars: cap_width must be >= 0");
    if (SciQLopPlots::posted_to_owner_thread(
            this, [this, lower = QPointer(lower), upper = QPointer(upper), cap_width] {
                if (lower && upper)
                    set_error_bars(lower, upper, cap_width);
            }))
        return;
    const auto ref = multi_ref();
    if (!ref)
    {
        WARN_UNSUPPORTED_FUNCTIONALITY;
        return;
    }
    if (!ref->graph->setComponentErrorBars(ref->componentIndex, sibling_index(lower),
                                           sibling_index(upper), cap_width))
        throw std::invalid_argument("error bars need three distinct components");
    Q_EMIT error_bars_changed();
    Q_EMIT replot();
}

void SciQLopGraphComponent::clear_error_bars()
{
    SCIQLOP_ON_OWNER_THREAD(clear_error_bars());
    const auto ref = multi_ref();
    if (!ref)
    {
        WARN_UNSUPPORTED_FUNCTIONALITY;
        return;
    }
    ref->graph->clearComponentErrorBars(ref->componentIndex);
    Q_EMIT error_bars_changed();
    Q_EMIT replot();
}

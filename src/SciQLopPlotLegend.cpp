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

#include "SciQLopPlots/SciQLopPlotLegend.hpp"
#include "SciQLopPlots/Debug.hpp"
#include <qcp.h>

SciQLopPlotLegend::SciQLopPlotLegend(QCPLegend* legend, QObject* parent)
        : SciQLopPlotLegendInterface(parent), m_legend(legend)
{
    legend->setSelectableParts(QCPLegend::spItems);
}

bool SciQLopPlotLegend::_visible() const
{
    return m_legend->visible();
}

void SciQLopPlotLegend::_set_visible(bool visible)
{
    m_legend->setVisible(visible);
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

namespace
{
// The legend lives in the axis rect's inset layout: its slot there holds its placement.
std::pair<QCPLayoutInset*, int> inset_slot(QCPLegend* legend)
{
    auto* inset = qobject_cast<QCPLayoutInset*>(legend->layout());
    if (inset)
        for (int i = 0; i < inset->elementCount(); ++i)
            if (inset->elementAt(i) == legend)
                return { inset, i };
    return { nullptr, -1 };
}
}

QPointF SciQLopPlotLegend::position() const
{
    auto [inset, index] = inset_slot(m_legend);
    if (!inset)
        return {};
    const QRect area = inset->rect();
    if (area.width() <= 0 || area.height() <= 0)
        return inset->insetRect(index).topLeft();
    const QRect legend = m_legend->outerRect();
    return { double(legend.x() - area.x()) / area.width(),
             double(legend.y() - area.y()) / area.height() };
}

void SciQLopPlotLegend::set_position(const QPointF& position)
{
    auto [inset, index] = inset_slot(m_legend);
    if (!inset)
        return;
    inset->setInsetPlacement(index, QCPLayoutInset::ipFree);
    // A zero size: the inset layout grows it to the legend's minimum, its natural size.
    inset->setInsetRect(index, QRectF(position, QSizeF(0., 0.)));
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    Q_EMIT position_changed(position);
}

void SciQLopPlotLegend::set_alignment(Qt::Alignment alignment)
{
    auto [inset, index] = inset_slot(m_legend);
    if (!inset)
        return;
    inset->setInsetPlacement(index, QCPLayoutInset::ipBorderAligned);
    inset->setInsetAlignment(index, alignment);
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    Q_EMIT position_changed(position());
}

Qt::Alignment SciQLopPlotLegend::alignment() const
{
    auto [inset, index] = inset_slot(m_legend);
    return inset ? inset->insetAlignment(index) : Qt::Alignment {};
}

void SciQLopPlotLegend::set_columns(int columns)
{
    if (columns <= 1)
    {
        m_legend->setFillOrder(QCPLayoutGrid::foRowsFirst);
        m_legend->setWrap(0);
    }
    else
    {
        m_legend->setFillOrder(QCPLayoutGrid::foColumnsFirst);
        m_legend->setWrap(columns);
    }
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

int SciQLopPlotLegend::columns() const
{
    return m_legend->fillOrder() == QCPLayoutGrid::foColumnsFirst ? std::max(1, m_legend->wrap()) : 1;
}

QFont SciQLopPlotLegend::font() const
{
    return m_legend->font();
}

void SciQLopPlotLegend::set_font(const QFont& font)
{
    if (m_legend->font() != font)
    {
        m_legend->setFont(font);
        m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT font_changed(font);
    }
}

QColor SciQLopPlotLegend::_color() const
{
    return m_legend->textColor();
}

void SciQLopPlotLegend::_set_color(const QColor& color)
{
    m_legend->setTextColor(color);
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

namespace
{
QString row_label(QCPAbstractLegendItem* item)
{
    if (auto* row = qobject_cast<QCPPlottableGroupLegendItem*>(item))
        return row->rowText(false);
    if (auto* row = qobject_cast<QCPGroupLegendItem*>(item))
        return row->headerName();
    if (auto* row = qobject_cast<QCPIntervalsLegendItem*>(item))
    {
        // A timeline's row lists its categories, one per line.
        QStringList names;
        if (row->intervals())
            for (const auto& [name, color] : row->intervals()->legendEntries())
                names.append(name);
        return names.join('\n');
    }
    if (auto* row = qobject_cast<QCPPlottableLegendItem*>(item))
        return row->plottable() ? row->plottable()->name() : QString();
    return {};
}
}

QStringList SciQLopPlotLegend::row_labels() const
{
    QStringList labels;
    for (int i = 0; i < m_legend->itemCount(); ++i)
        labels.append(row_label(m_legend->item(i)));
    return labels;
}

QRect SciQLopPlotLegend::row_rect(int row) const
{
    auto* item = row >= 0 && row < m_legend->itemCount() ? m_legend->item(row) : nullptr;
    if (!item)
        return {};
    QRect rect = item->outerRect();
    if (auto* plot = qobject_cast<QWidget*>(parent()))
        rect.moveTopLeft(m_legend->parentPlot()->mapTo(plot, rect.topLeft()));
    return rect;
}

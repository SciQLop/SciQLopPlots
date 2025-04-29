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
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp"
#include <QHBoxLayout>

SciQLopGraphInterface* SciQLopNDProjectionPlot::plot_impl(GetDataPyCallable callable,
                                                          QStringList labels, QList<QColor> colors,
                                                          GraphType graph_type, GraphMarkerShape marker, QObject* sync_with, QVariantMap metaData)
{
    SciQLopGraphInterface* graph
        = new SciQLopNDProjectionCurvesFunction(this, this->m_plots, std::move(callable), labels,metaData);

    connect(this, &SciQLopPlot::time_axis_range_changed, graph,
            &SciQLopNDProjectionCurvesFunction::set_range);
    Q_EMIT graph_list_changed();
    return graph;
}

SciQLopGraphInterface* SciQLopNDProjectionPlot::plot_impl(const QList<PyBuffer>& data,
                                                          QStringList labels, QList<QColor> colors,
                                                          GraphType graph_type, GraphMarkerShape marker, QVariantMap metaData)
{
    SciQLopGraphInterface* graph = new SciQLopNDProjectionCurves(this, this->m_plots, labels,metaData);
    graph->set_data(data);
    Q_EMIT graph_list_changed();
    return graph;
}

SciQLopNDProjectionPlot::SciQLopNDProjectionPlot(std::size_t projection_count, QWidget* parent)
{
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);
    for (auto i = 0UL; i < projection_count; i++)
    {
        auto plot = new SciQLopPlot(this);
        layout->addWidget(plot);
        m_plots.append(plot);
        if (i > 0)
        {
            plot->x_axis()->couple_range_with(m_plots[i - 1]->y_axis());
        }
    }
    if (projection_count > 2)
    {
        m_plots[0]->x_axis()->couple_range_with(m_plots.last()->y_axis());
    }
    m_time_axis = new SciQLopPlotDummyAxis(this);
    connect(m_time_axis, &SciQLopPlotAxis::range_changed, this,
            &SciQLopPlot::time_axis_range_changed);
}

void SciQLopNDProjectionPlot::set_linked_axes(bool linked) noexcept
{
    if (m_linked_axes != linked)
    {
        m_linked_axes = linked;
        for (decltype(m_plots.size()) i = 0; i < m_plots.size(); i++)
        {
            auto plot = m_plots[i];
            if (i > 0)
            {
                if (linked)
                    plot->x_axis()->couple_range_with(m_plots[i - 1]->y_axis());
                else
                    plot->x_axis()->decouple_range_from(m_plots[i - 1]->y_axis());
            }
        }
        if (m_plots.size() > 2)
        {
            if (linked)
                m_plots[0]->x_axis()->couple_range_with(m_plots.last()->y_axis());
            else
                m_plots[0]->x_axis()->decouple_range_from(m_plots.last()->y_axis());
        }
    }
}

SciQLopPlottableInterface* SciQLopNDProjectionPlot::plottable(int index)
{
    return plottables().at(index);
}

SciQLopPlottableInterface* SciQLopNDProjectionPlot::plottable(const QString& name)
{
    for (auto p : plottables())
    {
        if (p->name() == name)
            return p;
    }
    return nullptr;
}

QList<SciQLopPlottableInterface*> SciQLopNDProjectionPlot::plottables() const noexcept
{
    QList<SciQLopPlottableInterface*> plottables;
    for (auto c : children())
    {
        if (auto p = qobject_cast<SciQLopNDProjectionCurves*>(c); p)
        {
            plottables.append(p);
        }
    }
    return plottables;
}

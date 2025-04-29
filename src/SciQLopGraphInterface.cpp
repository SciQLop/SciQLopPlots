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
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/unique_names_factory.hpp"

SciQLopGraphInterface::SciQLopGraphInterface(const QString& prefix,QVariantMap metaData, QObject* parent)
        : SciQLopPlottableInterface(metaData, parent)
{
    connect(this, &QObject::objectNameChanged, this, &SciQLopGraphInterface::name_changed);
    setObjectName(UniqueNamesFactory::unique_name(prefix));
}


void SciQLopPlottableInterface::set_range(const SciQLopPlotRange& range)
{
    if (m_range != range)
    {
        m_range = range;
        Q_EMIT range_changed(range);
    }
}

SciQLopColorMapInterface::SciQLopColorMapInterface(QVariantMap metaData, QObject* parent)
        : SciQLopPlottableInterface(metaData, parent)
{
    setObjectName(UniqueNamesFactory::unique_name("ColorMap"));
}

void SciQLopFunctionGraph::observe(QObject* observable)
{
    while (m_observer_connections.size() > 0)
    {
        QObject::disconnect(m_observer_connections.takeLast());
    }
    if (auto axis = qobject_cast<SciQLopPlotAxisInterface*>(observable))
    {
        connect(axis, &SciQLopPlotAxisInterface::range_changed, as_graph,
                [this](const SciQLopPlotRange& range) { this->call(range); });
    }
    if (auto graph = qobject_cast<SciQLopGraphInterface*>(observable))
    {
        connect(graph, QOverload<PyBuffer, PyBuffer>::of(&SciQLopGraphInterface::data_changed),
                as_graph, [this](PyBuffer x, PyBuffer y) { this->call(x, y); });

        connect(graph,
                QOverload<PyBuffer, PyBuffer, PyBuffer>::of(&SciQLopGraphInterface::data_changed),
                as_graph, [this](PyBuffer x, PyBuffer y, PyBuffer z) { this->call(x, y, z); });

        connect(graph, QOverload<const QList<PyBuffer>&>::of(&SciQLopGraphInterface::data_changed),
                as_graph, [this](const QList<PyBuffer>& values) { this->call(values); });
    }
}

SciQLopFunctionGraph::SciQLopFunctionGraph(GetDataPyCallable&& callable,
                                           SciQLopPlottableInterface* as_graph, int N)
        : m_pipeline { new SimplePyCallablePipeline(std::move(callable), as_graph) }
        , as_graph { as_graph }
{
    switch (N)
    {
        case 2:
            QObject::connect(m_pipeline, &SimplePyCallablePipeline::new_data_2d, this->as_graph,
                             QOverload<PyBuffer, PyBuffer>::of(&SciQLopGraphInterface::set_data));
            break;
        case 3:
            QObject::connect(
                m_pipeline, &SimplePyCallablePipeline::new_data_3d, this->as_graph,
                QOverload<PyBuffer, PyBuffer, PyBuffer>::of(&SciQLopGraphInterface::set_data));
            break;
        default:
            QObject::connect(
                m_pipeline, &SimplePyCallablePipeline::new_data_nd, this->as_graph,
                QOverload<const QList<PyBuffer>&>::of(&SciQLopGraphInterface::set_data));
            break;
    }
    QObject::connect(this->as_graph, &SciQLopGraphInterface::range_changed, m_pipeline,
                     QOverload<const SciQLopPlotRange&>::of(&SimplePyCallablePipeline::call));
}

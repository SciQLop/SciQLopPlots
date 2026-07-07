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
#include "SciQLopPlots/Inspector/InspectorExtensionHolder.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/unique_names_factory.hpp"

#include <QDebug>

SciQLopPlottableInterface::SciQLopPlottableInterface(QVariantMap metaData, QObject* parent)
    : QObject(parent)
    , m_metaData { std::move(metaData) }
    , m_extension_holder { std::make_unique<InspectorExtensionHolder>(
          this, [this]() { Q_EMIT inspector_extensions_changed(); }) }
{
    connect(this, &QObject::objectNameChanged, this, &SciQLopPlottableInterface::name_changed);
}

SciQLopGraphInterface::SciQLopGraphInterface(const QString& prefix,QVariantMap metaData, QObject* parent)
        : SciQLopPlottableInterface(metaData, parent)
{
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

void SciQLopPlottableInterface::add_inspector_extension(InspectorExtension* extension)
{
    m_extension_holder->add(extension);
}

void SciQLopPlottableInterface::remove_inspector_extension(InspectorExtension* extension)
{
    m_extension_holder->remove(extension);
}

QList<InspectorExtension*> SciQLopPlottableInterface::inspector_extensions() const
{
    return m_extension_holder->list();
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
        connect(graph, QOverload<SciQLopPyBuffer, SciQLopPyBuffer>::of(&SciQLopGraphInterface::data_changed),
                as_graph, [this](SciQLopPyBuffer x, SciQLopPyBuffer y) { this->call(x, y); });

        connect(graph,
                QOverload<SciQLopPyBuffer, SciQLopPyBuffer, SciQLopPyBuffer>::of(&SciQLopGraphInterface::data_changed),
                as_graph, [this](SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z) { this->call(x, y, z); });

        connect(graph, QOverload<const QList<SciQLopPyBuffer>&>::of(&SciQLopGraphInterface::data_changed),
                as_graph, [this](const QList<SciQLopPyBuffer>& values) { this->call(values); });
    }
}

SciQLopFunctionGraph::SciQLopFunctionGraph(GetDataPyCallable&& callable,
                                           SciQLopPlottableInterface* as_graph, int N)
        : m_pipeline { new SimplePyCallablePipeline(std::move(callable), as_graph) }
        , as_graph { as_graph }
{
    connect_pipeline_data_to_graph(m_pipeline, this->as_graph, N);
    m_idle_connection = QObject::connect(m_pipeline, &SimplePyCallablePipeline::pipeline_idle,
                     this->as_graph, [graph = this->as_graph]() { graph->set_busy(false); });
    QObject::connect(this->as_graph, &SciQLopGraphInterface::range_changed, m_pipeline,
                     QOverload<const SciQLopPlotRange&>::of(&SimplePyCallablePipeline::call));
}

SciQLopRemoteGraph::SciQLopRemoteGraph(SciQLopPlottableInterface* as_graph, int N)
        : m_pipeline { new RemoteDataPipeline(as_graph) }, as_graph { as_graph }
{
    m_connections = connect_pipeline_data_to_graph(m_pipeline, this->as_graph, N);

    // Set busy when the range request goes out; clear busy when data arrives.
    // NOT on pipeline_idle: for remote providers pipeline_idle fires as soon as
    // the request is emitted (get_data returns immediately), before the response.
    // Drive busy through as_graph->set_busy, which virtual-dispatches to the
    // concrete remote graph's override (set_remote_busy + busy_changed). The
    // lambdas capture only QObjects (as_graph and its child pipeline) — never the
    // non-QObject mixin `this`, whose subobject is destroyed before the QObject
    // base, which would leave a queued delivery dereferencing a dangling pointer.
    m_connections << QObject::connect(this->as_graph,
        &SciQLopGraphInterface::range_changed, m_pipeline,
        [g = this->as_graph, pipeline = m_pipeline](const SciQLopPlotRange& range)
        { g->set_busy(true); pipeline->call(range); });

    // Clear busy on the same arity the data path is wired for, so a mismatched
    // signal can never drop busy without data having reached the graph.
    const auto clear_busy = [g = this->as_graph]() { g->set_busy(false); };
    switch (N)
    {
        case 2:
            m_connections << QObject::connect(m_pipeline, &RemoteDataPipeline::new_data_2d,
                                              this->as_graph, clear_busy);
            break;
        case 3:
            m_connections << QObject::connect(m_pipeline, &RemoteDataPipeline::new_data_3d,
                                              this->as_graph, clear_busy);
            break;
        default:
            m_connections << QObject::connect(m_pipeline, &RemoteDataPipeline::new_data_nd,
                                              this->as_graph, clear_busy);
            break;
    }
}

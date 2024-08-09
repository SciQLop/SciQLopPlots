/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Plotables/SciQLopLineGraph.hpp"
#include "SciQLopPlots/Plotables/SciQLopLineGraphResampler.hpp"

void SciQLopLineGraph::create_graphs(const QStringList& labels)
{
    if (plottable_count())
        clear_graphs();
    for (const auto& label : labels)
    {
        const auto graph = this->newPlottable<QCPGraph>(_keyAxis, _valueAxis, label);
        graph->setAdaptiveSampling(true);
    }
    _resampler->set_line_count(plottable_count());
}

void SciQLopLineGraph::set_auto_scale_y(bool auto_scale_y)
{
    _auto_scale_y = auto_scale_y;
    Q_EMIT auto_scale_y_changed(auto_scale_y);
}

void SciQLopLineGraph::_setGraphData(std::size_t index, QVector<QCPGraphData> data)
{
    auto graph = line(index);
    if (graph)
    {
        graph->data()->set(std::move(data), true);
        if (_auto_scale_y)
        {
            _valueAxis->rescale();
        }
    }
}

SciQLopLineGraph::SciQLopLineGraph(QCustomPlot* parent, QCPAxis* key_axis, QCPAxis* value_axis,
    const QStringList& labels, ::DataOrder data_order)
        : SQPQCPAbstractPlottableWrapper(parent)
        , _keyAxis { key_axis }
        , _valueAxis { value_axis }
        , _data_order { data_order }
{
    create_resampler(labels);
    if (!labels.isEmpty())
    {
        this->create_graphs(labels);
    }
}

void SciQLopLineGraph::clear_graphs(bool graph_already_removed)
{
    clear_plottables();
}

void SciQLopLineGraph::clear_resampler()
{
    connect(this->_resampler_thread, &QThread::finished, this->_resampler, &QThread::deleteLater);
    disconnect(this->_resampler, &LineGraphResampler::setGraphData, this,
        &SciQLopLineGraph::_setGraphData);
    disconnect(this->_resampler, &LineGraphResampler::refreshPlot, nullptr, nullptr);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopLineGraph::create_resampler(const QStringList& labels)
{
    this->_resampler = new LineGraphResampler(_data_order, std::size(labels));
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(this->_resampler, &LineGraphResampler::setGraphData, this,
        &SciQLopLineGraph::_setGraphData, Qt::QueuedConnection);
    connect(
        this->_resampler, &LineGraphResampler::refreshPlot, this,
        [this]() { this->_plot()->replot(QCustomPlot::rpQueuedReplot); }, Qt::QueuedConnection);
}


SciQLopLineGraph::~SciQLopLineGraph()
{
    clear_graphs();
    clear_resampler();
}

void SciQLopLineGraph::set_data(Array_view x, Array_view y)
{
    this->_resampler->setData(std::move(x), std::move(y));
}

SciQLopGraphFunction::SciQLopGraphFunction(QCustomPlot* parent, QCPAxis* key_axis,
    QCPAxis* value_axis, GetDataPyCallable&& callable, const QStringList& labels,
    DataOrder data_order)
        : SciQLopLineGraph(parent, key_axis, value_axis, labels, data_order)
{
    m_pipeline = new SimplePyCallablePipeline(std::move(callable), this);
    connect(
        m_pipeline, &SimplePyCallablePipeline::new_data_2d, this, &SciQLopGraphFunction::set_data);
    connect(
        this, &SciQLopLineGraph::range_changed, m_pipeline, &SimplePyCallablePipeline::set_range);
}

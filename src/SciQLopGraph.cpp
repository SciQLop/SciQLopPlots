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
#include "SciQLopPlots/Plotables/SciQLopGraph.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphResampler.hpp"

void SciQLopGraph::create_graphs(const QStringList& labels)
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

void SciQLopGraph::set_auto_scale_y(bool auto_scale_y)
{
    _auto_scale_y = auto_scale_y;
    Q_EMIT auto_scale_y_changed(auto_scale_y);
}

void SciQLopGraph::_range_changed(const QCPRange& new_range, const QCPRange& old_range)
{
    const auto data_x_range = this->_resampler->x_range();

    if (!(new_range.contains(data_x_range.lower) && new_range.contains(data_x_range.upper)
            && old_range.contains(data_x_range.lower) && old_range.contains(data_x_range.upper)))
        this->_resampler->resample(new_range);

    if (!(data_x_range.contains(new_range.lower) && data_x_range.contains(new_range.upper)))
        emit this->range_changed(new_range, true);
    else
        emit this->range_changed(new_range, false);
    emit this->range_changed(new_range.lower, new_range.upper);
}

void SciQLopGraph::_setGraphData(std::size_t index, QVector<QCPGraphData> data)
{
    auto graph = graphAt(index);
    if (graph)
    {
        graph->data()->set(std::move(data), true);
        if (_auto_scale_y)
        {
            _valueAxis->rescale();
        }
    }
}

SciQLopGraph::SciQLopGraph(QCustomPlot* parent, QCPAxis* key_axis, QCPAxis* value_axis,
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
    connect(key_axis, QOverload<const QCPRange&, const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        QOverload<const QCPRange&, const QCPRange&>::of(&SciQLopGraph::_range_changed));
}

void SciQLopGraph::clear_graphs(bool graph_already_removed)
{
    clear_plottables();
}

void SciQLopGraph::clear_resampler()
{
    connect(this->_resampler_thread, &QThread::finished, this->_resampler, &QThread::deleteLater);
    disconnect(this->_resampler, &GraphResampler::setGraphData, this, &SciQLopGraph::_setGraphData);
    disconnect(this->_resampler, &GraphResampler::refreshPlot, nullptr, nullptr);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopGraph::create_resampler(const QStringList& labels)
{
    this->_resampler = new GraphResampler(_data_order, std::size(labels));
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(this->_resampler, &GraphResampler::setGraphData, this, &SciQLopGraph::_setGraphData,
        Qt::QueuedConnection);
    connect(
        this->_resampler, &GraphResampler::refreshPlot, this,
        [this]() { this->_plot()->replot(QCustomPlot::rpQueuedReplot); }, Qt::QueuedConnection);
}


SciQLopGraph::~SciQLopGraph()
{
    clear_graphs();
    clear_resampler();
}

void SciQLopGraph::set_data(Array_view x, Array_view y)
{
    this->_resampler->setData(std::move(x), std::move(y));
}

SciQLopGraphFunction::SciQLopGraphFunction(QCustomPlot* parent, QCPAxis* key_axis,
    QCPAxis* value_axis, GetDataPyCallable&& callable, const QStringList& labels,
    DataOrder data_order)
        : SciQLopGraph(parent, key_axis, value_axis, labels, data_order)
{
    m_pipeline = new SimplePyCallablePipeline(std::move(callable), this);
    connect(
        m_pipeline, &SimplePyCallablePipeline::new_data_2d, this, &SciQLopGraphFunction::set_data);
    connect(this, QOverload<double, double>::of(&SciQLopGraph::range_changed), m_pipeline,
        &SimplePyCallablePipeline::set_range);
}

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
#include "SciQLopPlots/Plotables/Resamplers/SciQLopLineGraphResampler.hpp"

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


void SciQLopLineGraph::_setGraphData(QList<QVector<QCPGraphData>> data)
{
    for (std::size_t i = 0; i < plottable_count(); i++)
    {
        auto graph = line(i);
        if (graph)
        {
            graph->data()->set(data[i], true);
        }
    }
    Q_EMIT this->replot();
}

SciQLopLineGraph::SciQLopLineGraph(
    QCustomPlot* parent, QCPAxis* key_axis, QCPAxis* value_axis, const QStringList& labels)
        : SQPQCPAbstractPlottableWrapper(parent), _keyAxis { key_axis }, _valueAxis { value_axis }
{
    create_resampler(labels);
    if (!labels.isEmpty())
    {
        this->create_graphs(labels);
    }
    connect(key_axis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this->_resampler,
        &LineGraphResampler::resample);
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
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopLineGraph::create_resampler(const QStringList& labels)
{
    this->_resampler = new LineGraphResampler(std::size(labels));
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(this->_resampler, &LineGraphResampler::setGraphData, this,
        &SciQLopLineGraph::_setGraphData, Qt::QueuedConnection);
}


SciQLopLineGraph::~SciQLopLineGraph()
{
    clear_graphs();
    clear_resampler();
}

void SciQLopLineGraph::set_data(PyBuffer x, PyBuffer y)
{
    this->_resampler->setData(x, y);
    Q_EMIT data_changed(x, y);
}

QList<PyBuffer> SciQLopLineGraph::data() const noexcept
{
    return _resampler->get_data();
}

SciQLopLineGraphFunction::SciQLopLineGraphFunction(QCustomPlot* parent, QCPAxis* key_axis,
    QCPAxis* value_axis, GetDataPyCallable&& callable, const QStringList& labels)
        : SciQLopLineGraph(parent, key_axis, value_axis, labels)
{
    m_pipeline = new SimplePyCallablePipeline(std::move(callable), this);
    connect(m_pipeline, &SimplePyCallablePipeline::new_data_2d, this,
        &SciQLopLineGraphFunction::_set_data);
    connect(
        this, &SciQLopLineGraph::range_changed, m_pipeline, &SimplePyCallablePipeline::set_range);
}

void SciQLopLineGraphFunction::set_data(PyBuffer x, PyBuffer y)
{
    m_pipeline->set_data(x, y);
}

void SciQLopLineGraphFunction::set_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    m_pipeline->set_data(x, y, z);
}

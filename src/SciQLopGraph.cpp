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
#include "SciQLopPlots/SciQLopGraph.hpp"
#include "SciQLopPlots/SciQLopGraphResampler.hpp"

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

void SciQLopGraph::_range_changed(const QCPRange& newRange, const QCPRange& oldRange)
{
    const auto data_x_range = this->_resampler->x_range();

    if (!(newRange.contains(data_x_range.lower) && newRange.contains(data_x_range.upper)
            && oldRange.contains(data_x_range.lower) && oldRange.contains(data_x_range.upper)))
        this->_resampler->resample(newRange);

    if (!(data_x_range.contains(newRange.lower) && data_x_range.contains(newRange.upper)))
        emit this->range_changed(newRange, true);
    else
        emit this->range_changed(newRange, false);
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

SciQLopGraph::SciQLopGraph(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
    const QStringList& labels, DataOrder dataOrder)
        : SQPQCPAbstractPlottableWrapper(parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
        , _dataOrder { dataOrder }
{

    create_resampler(labels);
    this->create_graphs(labels);
    connect(keyAxis, QOverload<const QCPRange&, const QCPRange&>::of(&QCPAxis::rangeChanged), this,
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
    this->_resampler = new GraphResampler(_dataOrder, std::size(labels));
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(this->_resampler, &GraphResampler::setGraphData, this, &SciQLopGraph::_setGraphData,
        Qt::QueuedConnection);
    connect(
        this->_resampler, &GraphResampler::refreshPlot, this,
        [this]() { this->_plot()->replot(QCustomPlot::rpQueuedReplot); }, Qt::QueuedConnection);
}


SciQLopGraph::SciQLopGraph(
    QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis, DataOrder dataOrder)
        : SQPQCPAbstractPlottableWrapper(parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
        , _dataOrder { dataOrder }
{
    create_resampler({});
    connect(keyAxis, QOverload<const QCPRange&, const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        QOverload<const QCPRange&, const QCPRange&>::of(&SciQLopGraph::_range_changed));
}

SciQLopGraph::~SciQLopGraph()
{
    clear_graphs();
    clear_resampler();
}

void SciQLopGraph::setData(Array_view&& x, Array_view&& y, bool ignoreCurrentRange)
{
    this->_resampler->setData(std::move(x), std::move(y));
}

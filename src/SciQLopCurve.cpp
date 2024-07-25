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
#include "SciQLopPlots/SciQLopCurve.hpp"
#include "SciQLopPlots/SciQLopCurveResampler.hpp"

void SciQLopCurve::_range_changed(const QCPRange& newRange, const QCPRange& oldRange)
{
    this->_resampler->resample(newRange);
    emit this->range_changed(newRange, false);
}

void SciQLopCurve::_setCurveData(std::size_t index, QVector<QCPCurveData> data)
{
    if (index < std::size(_curves))
        _curves[index]->data()->set(std::move(data), true);
}

void SciQLopCurve::clear_curves(bool curve_already_removed)
{
    if (!curve_already_removed)
    {
        for (auto curve : _curves)
        {
            this->_plot()->removePlottable(curve);
        }
    }
    this->_curves.clear();
}

void SciQLopCurve::clear_resampler()
{
    connect(this->_resampler_thread, &QThread::finished, this->_resampler, &QThread::deleteLater);
    disconnect(this->_resampler, &CurveResampler::setGraphData, this, &SciQLopCurve::_setCurveData);
    disconnect(this->_resampler, &CurveResampler::refreshPlot, nullptr, nullptr);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopCurve::create_resampler(const QStringList& labels)
{
    this->_resampler = new CurveResampler(_dataOrder, std::size(labels));
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(this->_resampler, &CurveResampler::setGraphData, this, &SciQLopCurve::_setCurveData,
        Qt::QueuedConnection);
    connect(
        this->_resampler, &CurveResampler::refreshPlot, this,
        [this]() { this->_plot()->replot(QCustomPlot::rpQueuedReplot); }, Qt::QueuedConnection);
}

void SciQLopCurve::curve_got_removed_from_plot(QCPCurve* curve)
{
    this->_curves.removeOne(curve);
}

SciQLopCurve::SciQLopCurve(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
    const QStringList& labels, SciQLopGraph::DataOrder dataOrder)
        : SQPQCPAbstractPlottableWrapper(parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
        , _dataOrder { dataOrder }
{
    create_resampler(labels);
    this->create_graphs(labels);
}

SciQLopCurve::SciQLopCurve(
    QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis, SciQLopGraph::DataOrder dataOrder)
        : SQPQCPAbstractPlottableWrapper(parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
        , _dataOrder { dataOrder }
{
    create_resampler({});
}

SciQLopCurve::~SciQLopCurve()
{
    clear_curves();
    clear_resampler();
}

void SciQLopCurve::setData(Array_view&& x, Array_view&& y, bool ignoreCurrentRange)
{
    this->_resampler->setData(std::move(x), std::move(y));
}

void SciQLopCurve::create_graphs(const QStringList& labels)
{
    if (std::size(_curves))
        clear_curves();
    for (const auto& label : labels)
    {
        const auto curve = this->newPlottable<QCPCurve>(_keyAxis, _valueAxis, label);
        _curves.append(curve);
        connect(curve, &QCPCurve::destroyed, this,
            [this, curve]() { this->curve_got_removed_from_plot(curve); });
    }
    _resampler->set_line_count(std::size(_curves));
}

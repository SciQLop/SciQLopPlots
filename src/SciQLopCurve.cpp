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
#include "SciQLopPlots/Plotables/SciQLopCurve.hpp"
#include "SciQLopPlots/Plotables/Resamplers/SciQLopCurveResampler.hpp"

void SciQLopCurve::_range_changed(const QCPRange& newRange, const QCPRange& oldRange)
{
    this->_resampler->resample(newRange);
}

void SciQLopCurve::_setCurveData(QList<QVector<QCPCurveData>> data)
{
    for (std::size_t i = 0; i < plottable_count(); i++)
    {
        auto curve = line(i);
        if (curve)
            curve->data()->set(data[i], true);
    }
    Q_EMIT this->replot();
    Q_EMIT data_changed();
}

void SciQLopCurve::clear_curves(bool curve_already_removed)
{
    clear_plottables();
}

void SciQLopCurve::clear_resampler()
{
    connect(this->_resampler_thread, &QThread::finished, this->_resampler, &QThread::deleteLater);
    disconnect(this->_resampler, &CurveResampler::setGraphData, this, &SciQLopCurve::_setCurveData);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopCurve::create_resampler(const QStringList& labels)
{
    this->_resampler = new CurveResampler(this, std::size(labels));
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(this->_resampler, &CurveResampler::setGraphData, this, &SciQLopCurve::_setCurveData,
            Qt::QueuedConnection);
}

SciQLopCurve::SciQLopCurve(QCustomPlot* parent, SciQLopPlotAxis* keyAxis,
                           SciQLopPlotAxis* valueAxis, const QStringList& labels, QVariantMap metaData)
        : SQPQCPAbstractPlottableWrapper("Curve",metaData, parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
{
    create_resampler(labels);
    this->create_graphs(labels);
}

SciQLopCurve::SciQLopCurve(QCustomPlot* parent, SciQLopPlotAxis* keyAxis,
                           SciQLopPlotAxis* valueAxis, QVariantMap metaData)
        : SQPQCPAbstractPlottableWrapper("Curve", metaData, parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
{
    create_resampler({});
}

SciQLopCurve::~SciQLopCurve()
{
    clear_curves();
    clear_resampler();
}

void SciQLopCurve::set_data(PyBuffer x, PyBuffer y)
{
    this->_resampler->setData(x, y);
    Q_EMIT data_changed(x, y);
}

QList<PyBuffer> SciQLopCurve::data() const noexcept
{
    return _resampler->get_data();
}

void SciQLopCurve::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        this->_keyAxis = qcp_axis;
        for (auto plottable : m_components)
        {
            auto curve = qobject_cast<QCPCurve*>(plottable->plottable());
            curve->setKeyAxis(qcp_axis->qcp_axis());
        }
    }
}

void SciQLopCurve::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        this->_valueAxis = qcp_axis;
        for (auto plottable : m_components)
        {
            auto curve = qobject_cast<QCPCurve*>(plottable->plottable());
            curve->setValueAxis(qcp_axis->qcp_axis());
        }
    }
}

void SciQLopCurve::create_graphs(const QStringList& labels)
{
    if (plottable_count())
        clear_curves();
    for (const auto& label : labels)
    {
        this->newComponent<QCPCurve>(_keyAxis->qcp_axis(), _valueAxis->qcp_axis(), label);
    }
    _resampler->set_line_count(plottable_count());
}

SciQLopCurveFunction::SciQLopCurveFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                           SciQLopPlotAxis* value_axis,
                                           GetDataPyCallable&& callable, const QStringList& labels, QVariantMap metaData)
        : SciQLopCurve { parent, key_axis, value_axis, labels,metaData }
        , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({ parent->xAxis->range().lower, parent->xAxis->range().upper });
}

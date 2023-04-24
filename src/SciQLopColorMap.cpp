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
#include "SciQLopPlots/SciQLopColorMap.hpp"

#include "SciQLopPlots/SciQLopColorMapResampler.hpp"
#include <cpp_utils/containers/algorithms.hpp>

void SciQLopColorMap::_range_changed(const QCPRange& newRange, const QCPRange& oldRange) { }


void SciQLopColorMap::_cmap_got_destroyed()
{
    this->_cmap = nullptr;
}

SciQLopColorMap::SciQLopColorMap(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
    const QString& name, DataOrder dataOrder)
        : QObject(parent), _keyAxis { keyAxis }, _valueAxis { valueAxis }, _dataOrder { dataOrder }
{
    this->_cmap = new QCPColorMap(keyAxis, valueAxis);
    this->_cmap->setName(name);
    connect(keyAxis, QOverload<const QCPRange&, const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        QOverload<const QCPRange&, const QCPRange&>::of(&SciQLopColorMap::_range_changed));
    connect(this->_cmap, &QCPColorMap::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);

    this->_resampler = new ColormapResampler(_valueAxis->scaleType());
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(
        this->_resampler, &ColormapResampler::refreshPlot, this,
        [this](QCPColorMapData* data)
        {
            this->colorMap()->setData(data, false);
            this->_plot()->replot(QCustomPlot::rpQueuedReplot);
        },
        Qt::QueuedConnection);
}

SciQLopColorMap::~SciQLopColorMap()
{
    if (this->_cmap)
        this->_plot()->removePlottable(this->colorMap());

    connect(this->_resampler_thread, &QThread::finished, this->_resampler, &QThread::deleteLater);
    disconnect(this->_resampler, &ColormapResampler::refreshPlot, nullptr, nullptr);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopColorMap::setData(NpArray_view&& x, NpArray_view&& y, NpArray_view&& z)
{
    if (this->_cmap)
    {
        this->_resampler->setData(
            std::move(x), std::move(y), std::move(z), _valueAxis->scaleType());
    }
    this->_plot()->replot(QCustomPlot::rpQueuedReplot);
}

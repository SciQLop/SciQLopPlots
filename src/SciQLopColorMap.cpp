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
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"

#include "SciQLopPlots/Plotables/SciQLopColorMapResampler.hpp"
#include <cpp_utils/containers/algorithms.hpp>

void SciQLopColorMap::_cmap_got_destroyed()
{
    this->_cmap = nullptr;
}

SciQLopColorMap::SciQLopColorMap(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
    const QString& name, ::DataOrder dataOrder)
        : SQPQCPAbstractPlottableWrapper(parent)
        , _icon_update_timer { new QTimer(this) }
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
        , _dataOrder { dataOrder }
{
    this->_cmap = this->newPlottable<QCPColorMap>(keyAxis, valueAxis, name);
    connect(this->_cmap, &QCPColorMap::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);

    this->_resampler = new ColormapResampler(_valueAxis->scaleType());
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    this->_icon_update_timer->setInterval(1000);
    this->_icon_update_timer->setSingleShot(true);
    connect(
        this->_icon_update_timer, &QTimer::timeout, this->_cmap,
        [this]() { this->_cmap->updateLegendIcon(); }, Qt::QueuedConnection);
    connect(
        this->_resampler, &ColormapResampler::refreshPlot, this,
        [this](QCPColorMapData* data)
        {
            this->colorMap()->setData(data, false);
            if (this->_auto_scale_y)
            {
                this->_valueAxis->rescale(true);
            }
            this->_plot()->replot(QCustomPlot::rpQueuedReplot);
            this->_icon_update_timer->start();
        },
        Qt::QueuedConnection);
    this->colorMap()->updateLegendIcon();
    this->colorMap()->setLayer(parent->layer("background"));
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

void SciQLopColorMap::set_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    if (this->_cmap)
    {
        this->_resampler->setData(x, y, z, _valueAxis->scaleType());
    }
    Q_EMIT data_changed(x, y, z);
}

void SciQLopColorMap::set_auto_scale_y(bool auto_scale_y)
{
    _auto_scale_y = auto_scale_y;
    Q_EMIT auto_scale_y_changed(auto_scale_y);
}

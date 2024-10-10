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

#include "SciQLopPlots/Plotables/Resamplers/SciQLopColorMapResampler.hpp"
#include <cpp_utils/containers/algorithms.hpp>

void SciQLopColorMap::_cmap_got_destroyed()
{
    this->_cmap = nullptr;
}

void SciQLopColorMap::_setGraphData(QCPColorMapData* data)
{
    if (this->_cmap)
    {
        this->_cmap->setData(data, false);
        Q_EMIT this->replot();
        this->_icon_update_timer->start();
    }
}

SciQLopColorMap::SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* keyAxis,
                                 SciQLopPlotAxis* valueAxis, const QString& name)
        : SciQLopColorMapInterface(parent)
        , _icon_update_timer { new QTimer(this) }
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
{
    this->_cmap = new QCPColorMap(this->_keyAxis->qcp_axis(), this->_valueAxis->qcp_axis());
    connect(this->_cmap, &QCPColorMap::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);
    this->set_gradient(ColorGradient::Jet);
    this->set_name(name);

    this->_resampler = new ColormapResampler(_valueAxis->qcp_axis()->scaleType());
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    this->_icon_update_timer->setInterval(1000);
    this->_icon_update_timer->setSingleShot(true);
    connect(
        this->_icon_update_timer, &QTimer::timeout, this->_cmap,
        [this]() { this->_cmap->updateLegendIcon(); }, Qt::QueuedConnection);
    connect(this->_valueAxis->qcp_axis(), &QCPAxis::scaleTypeChanged, this->_resampler,
            &ColormapResampler::setScaleType, Qt::DirectConnection);
    connect(this->_resampler, &ColormapResampler::setGraphData, this,
            &SciQLopColorMap::_setGraphData, Qt::QueuedConnection);
    this->colorMap()->updateLegendIcon();
    this->colorMap()->setLayer(parent->layer("background"));

    if (auto legend_item = _legend_item(); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                &SciQLopColorMap::set_selected);
    }
}

SciQLopColorMap::~SciQLopColorMap()
{
    if (this->_cmap)
        this->_plot()->removePlottable(this->colorMap());

    connect(this->_resampler_thread, &QThread::finished, this->_resampler, &QThread::deleteLater);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;

    if (auto legend_item = _legend_item(); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                &SciQLopColorMap::set_selected);
    }
}

void SciQLopColorMap::set_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    if (this->_cmap)
    {
        this->_resampler->setData(x, y, z);
    }
    Q_EMIT data_changed(x, y, z);
}

void SciQLopColorMap::set_auto_scale_y(bool auto_scale_y)
{
    _auto_scale_y = auto_scale_y;
    Q_EMIT auto_scale_y_changed(auto_scale_y);
}

void SciQLopColorMap::set_selected(bool selected) noexcept
{
    if (_selected != selected)
    {
        _selected = selected;
        auto legend_item = _legend_item();
        if (legend_item && legend_item->selected() != selected)
            legend_item->setSelected(selected);
        emit selection_changed(selected);
    }
}

bool SciQLopColorMap::selected() const noexcept
{
    return _selected;
}

void SciQLopColorMap::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        this->_keyAxis = qcp_axis;
        this->_cmap->setKeyAxis(this->_keyAxis->qcp_axis());
    }
}

void SciQLopColorMap::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        this->_valueAxis = qcp_axis;
        this->_cmap->setValueAxis(this->_valueAxis->qcp_axis());
    }
}

SciQLopColorMapFunction::SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                                 SciQLopPlotAxis* value_axis,
                                                 GetDataPyCallable&& callable, const QString& name)
        : SciQLopColorMap(parent, key_axis, value_axis, name)
{
    m_pipeline = new SimplePyCallablePipeline(std::move(callable), this);
    connect(m_pipeline, &SimplePyCallablePipeline::new_data_3d, this,
            &SciQLopColorMapFunction::_set_data);
    connect(this, &SciQLopColorMap::range_changed, m_pipeline,
            &SimplePyCallablePipeline::set_range);
}

void SciQLopColorMapFunction::set_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    m_pipeline->set_data(x, y, z);
}

void SciQLopColorMapFunction::set_data(PyBuffer x, PyBuffer y)
{
    m_pipeline->set_data(x, y);
}

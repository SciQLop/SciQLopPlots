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
#include "SciQLopPlots/constants.hpp"
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
        if (!_got_first_data && data->keySize() > 0)
        {
            _got_first_data = true;
            Q_EMIT request_rescale();
        }
    }
    Q_EMIT data_changed();
}

SciQLopColorMap::SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                 SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                 const QString& name, QVariantMap metaData)
        : SciQLopColorMapInterface(metaData, parent)
        , _icon_update_timer { new QTimer(this) }
        , _keyAxis { xAxis }
        , _valueAxis { yAxis }
        , _colorScaleAxis { zAxis }
{
    this->_cmap = new QCPColorMap(this->_keyAxis->qcp_axis(), this->_valueAxis->qcp_axis());
    this->_cmap->setLayer(Constants::LayersNames::ColorMap);
    connect(this->_cmap, &QCPColorMap::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);
    SciQLopColorMap::set_gradient(ColorGradient::Jet);
    SciQLopColorMap::set_name(name);

    this->_resampler = new ColormapResampler(this, _valueAxis->log());
    this->_resampler_thread = new QThread();
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    this->_icon_update_timer->setInterval(1000);
    this->_icon_update_timer->setSingleShot(true);
    connect(
        this->_icon_update_timer, &QTimer::timeout, this->_cmap,
        [this]() { this->_cmap->updateLegendIcon(); }, Qt::QueuedConnection);
    connect(
        this->_valueAxis, &SciQLopPlotAxis::log_changed, this->_resampler,
        [this](bool log) { this->_resampler->set_y_scale_log(log); }, Qt::DirectConnection);
    connect(this->_resampler, &ColormapResampler::setGraphData, this,
            &SciQLopColorMap::_setGraphData, Qt::QueuedConnection);

    this->colorMap()->updateLegendIcon();

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

QList<PyBuffer> SciQLopColorMap::data() const noexcept
{
    if (this->_cmap)
    {
        return this->_resampler->get_data();
    }
    return {};
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

SciQLopColorMapFunction::SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                                 SciQLopPlotAxis* yAxis,
                                                 SciQLopPlotColorScaleAxis* zAxis,
                                                 GetDataPyCallable&& callable, const QString& name)
        : SciQLopColorMap { parent, xAxis, yAxis, zAxis, name }
        , SciQLopFunctionGraph(std::move(callable), this, 3)
{
    this->set_range({ parent->xAxis->range().lower, parent->xAxis->range().upper });
}

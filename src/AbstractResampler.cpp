/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp"

void AbstractResampler1d::_async_resample()
{
    _async_resample_callback();
}

AbstractResampler1d::AbstractResampler1d(SciQLopPlottableInterface* parent, std::size_t line_cnt)
        : QObject(), _line_cnt { line_cnt }
{
    connect(this, &AbstractResampler1d::_resample_sig, this, &AbstractResampler1d::_async_resample,
            Qt::QueuedConnection);
    connect(parent, &SciQLopPlottableInterface::parent_plot_resized, this,
            [this](const QSize& size) { this->set_plot_size(size); });
    this->set_plot_size(parent->parent_plot_size());
}

void AbstractResampler1d::resample(const QCPRange new_range)
{
    this->set_next_plot_range(new_range);
    emit _resample_sig();
}

void AbstractResampler2d::_async_resample()
{
    _async_resample_callback();
}

AbstractResampler2d::AbstractResampler2d(SciQLopPlottableInterface* parent) : QObject()
{
    connect(this, &AbstractResampler2d::_resample_sig, this, &AbstractResampler2d::_async_resample,
            Qt::QueuedConnection);
    connect(parent, &SciQLopPlottableInterface::parent_plot_resized, this,
            [this](const QSize& size) { this->set_plot_size(size); });
    this->set_plot_size(parent->parent_plot_size());
}

void AbstractResampler2d::resample(const QCPRange new_range)
{
    this->set_next_plot_range(new_range);
    emit _resample_sig();
}

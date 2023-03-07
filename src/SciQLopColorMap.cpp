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
#include <cpp_utils/containers/algorithms.hpp>

void SciQLopColorMap::_range_changed(const QCPRange& newRange, const QCPRange& oldRange) { }

void SciQLopColorMap::_setDataLinear()
{
    if (std::size(_x) && std::size(_y) && std::size(_z))
    {
        using namespace cpp_utils;
        auto data = new QCPColorMapData(std::size(_x), std::size(_y),
            { *containers::min(_x), *containers::max(_x) },
            { *containers::min(_y), *containers::max(_y) });
        auto it = std::cbegin(_z);
        for (const auto i : _x)
        {
            for (const auto j : _y)
            {
                if (it != std::cend(_z))
                {
                    data->setData(i, j, *it);
                    it++;
                }
            }
        }
        this->_cmap->setData(data, false);
    }
}

void SciQLopColorMap::_setDataLog()
{
    using namespace cpp_utils;
    auto data = new QCPColorMapData(std::size(_x), std::size(_y),
        { *containers::min(_x), *containers::max(_x) },
        { *containers::min(_y), *containers::max(_y) });
    auto it = std::cbegin(_z);
    for (auto i = 0UL; i < std::size(_x); i++)
    {
        for (auto j = 0UL; j < std::size(_y); j++)
        {
            if (it != std::cend(_z))
            {
                data->setCell(i, j, *it);
                it++;
            }
        }
    }
    this->_cmap->setData(data, false);
}

SciQLopColorMap::SciQLopColorMap(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
    const QString& name, DataOrder dataOrder)
        : QObject(parent), _keyAxis { keyAxis }, _valueAxis { valueAxis }, _dataOrder { dataOrder }
{
    this->_cmap = new QCPColorMap(keyAxis, valueAxis);
    this->_cmap->setName(name);
    connect(keyAxis, QOverload<const QCPRange&, const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        QOverload<const QCPRange&, const QCPRange&>::of(&SciQLopColorMap::_range_changed));
}

SciQLopColorMap::~SciQLopColorMap()
{
    this->_plot()->removePlottable(this->colorMap());
}

void SciQLopColorMap::setData(NpArray_view&& x, NpArray_view&& y, NpArray_view&& z)
{
    {
        QMutexLocker locker(&_data_swap_mutex);
        _x = std::move(x);
        _y = std::move(y);
        _z = std::move(z);

        if (std::size(_x) && std::size(_y) && std::size(_z))
        {
            if (this->_cmap->valueAxis()->scaleType() == QCPAxis::stLinear)
            {
                this->_setDataLinear();
            }
            else
            {
                this->_setDataLog();
            }
        }
        else
        {
            this->_cmap->setData(new QCPColorMapData(0, 0, { 0., 0. }, { 0., 0. }), false);
        }
    }
    this->_plot()->replot(QCustomPlot::rpQueuedReplot);
}

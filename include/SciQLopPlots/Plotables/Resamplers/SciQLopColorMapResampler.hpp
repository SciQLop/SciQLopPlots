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
#pragma once

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "AbstractResampler.hpp"


#include <QAtomicInteger>
#include <QMutex>
#include <cpp_utils/containers/algorithms.hpp>
#include <qcustomplot.h>


struct ColormapResampler : public AbstractResampler2d
{
    Q_OBJECT
    QAtomicInteger<bool> _log_scale = false;
    std::size_t _max_x_size = 1000;
    std::size_t _max_y_size = 1000;

    void _resample_impl(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
        const QCPRange new_range, bool new_data);

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(QCPColorMapData* data);
#endif // !BINDINGS_H

    ColormapResampler(QCPAxis::ScaleType scale_type);
    ~ColormapResampler();

    inline void setScaleType(QCPAxis::ScaleType scale_type)
    {
        _log_scale.storeRelaxed(scale_type == QCPAxis::stLogarithmic);
    }
    inline QCPAxis::ScaleType scaleType() const
    {
        return _log_scale.loadRelaxed() ? QCPAxis::stLogarithmic : QCPAxis::stLinear;
    }
};

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
#pragma once

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "AbstractResampler.hpp"
#include <QMutex>
#include <qcustomplot.h>

struct CurveResampler : public AbstractResampler1d
{
    Q_OBJECT

    void _resample_impl(const ResamplerData1d& data, const ResamplerPlotInfo& plot_info);

public:
#ifndef BINDINGS_H
    Q_SIGNAL void setGraphData(QList<QVector<QCPCurveData>> data);
#endif // !BINDINGS_H

    CurveResampler(SciQLopPlottableInterface* parent, std::size_t line_cnt);
};

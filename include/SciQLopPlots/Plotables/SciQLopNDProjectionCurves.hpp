/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopLineGraph.hpp"
#include "SciQLopPlots/enums.hpp"
#include <qcustomplot.h>

#include "SciQLopPlots/Plotables/SciQLopCurve.hpp"


class SciQLopNDProjectionCurves : public SciQLopGraphInterface
{
    Q_OBJECT
    QList<SciQLopCurve*> m_curves;


public:
    explicit SciQLopNDProjectionCurves(SciQLopPlotInterface* parent, QList<SciQLopPlot*>& plots, const QStringList& labels, QVariantMap metaData={});
    virtual ~SciQLopNDProjectionCurves() override = default;

    Q_SLOT virtual void set_data(const QList<PyBuffer>& data) override;
    virtual void set_selected(bool selected) noexcept override;
    virtual  bool selected() const noexcept override;
};

class SciQLopNDProjectionCurvesFunction :public SciQLopNDProjectionCurves, public SciQLopFunctionGraph
{
    Q_OBJECT

    inline Q_SLOT void _set_data(QList<PyBuffer> data)
    {
        SciQLopNDProjectionCurves::set_data(data);
    }

public:
    explicit SciQLopNDProjectionCurvesFunction(SciQLopPlotInterface* parent, QList<SciQLopPlot*>& plots,
                                  GetDataPyCallable&& callable, const QStringList& labels, QVariantMap metaData={});

    virtual ~SciQLopNDProjectionCurvesFunction() override = default;


};

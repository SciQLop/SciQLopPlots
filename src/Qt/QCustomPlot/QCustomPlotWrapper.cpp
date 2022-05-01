/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2020, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Qt/QCustomPlot/QCustomPlotWrapper.hpp"

void SciQLopPlots::QCPWrappers::QCustomPlotWrapper::_plot_slt(int graphIndex, const QVector<QCPGraphData>& data)
{
    graph(graphIndex)->data()->set(data, true);
    replot(20);
    emit dataChanged();
}

void SciQLopPlots::QCPWrappers::QCustomPlotWrapper::_plot_slt(QCPColorMapData* data)
{
    if (m_colormap)
    {
        m_colormap->setData(data, false);
        replot(20);
        emit dataChanged();
    }
}


Q_DECLARE_METATYPE(QVector<QCPGraphData>)
namespace
{

struct MetaTypeAutoreg
{
    MetaTypeAutoreg() { qRegisterMetaType<QVector<QCPGraphData>>(); }
};
static MetaTypeAutoreg _;
}
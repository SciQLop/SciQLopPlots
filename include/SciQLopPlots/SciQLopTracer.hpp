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
#include <SciQLopPlots/QCPItemRichText.hpp>
#include <qcustomplot.h>

class TracerWithToolTip
{
    QCPItemRichText* m_tooltip = nullptr;
    QCPItemTracer* m_tracer = nullptr;
    double m_x = 0;
    double m_y = 0;

public:
    TracerWithToolTip(QCustomPlot* parent = nullptr);

    virtual ~TracerWithToolTip();

    void update_position(const QPointF& pos, bool replot = true);

    inline void set_visible(bool visible)
    {
        m_tracer->setVisible(visible);
        m_tooltip->setVisible(visible);
    }

    inline bool visible() const noexcept { return m_tracer->visible(); }
    inline void replot() { m_tracer->layer()->replot(); }

    inline void set_graph(QCPGraph* graph)
    {
        if (graph == nullptr && m_tracer->graph() != nullptr)
        {
            m_tracer->setGraph(nullptr);
            set_visible(false);
            replot();
        }
        else
        {
            if (m_tracer->graph() != graph)
            {
                m_tracer->setGraph(graph);
                m_tracer->setPen(graph->pen());
            }
        }
    }
};

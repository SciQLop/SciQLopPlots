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
#include <SciQLopPlots/SciQLopColorMap.hpp>


#include "constants.hpp"
#include <QPointF>
#include <SciQLopPlots/SciQLopCurve.hpp>
#include <SciQLopPlots/SciQLopGraph.hpp>
#include <SciQLopPlots/SciQLopPlotItem.hpp>
#include <optional>
#include <qcustomplot.h>

class SciQLopPlot : public QCustomPlot
{
    Q_OBJECT

    double m_scroll_factor = 1.;

public:
#ifndef BINDINGS_H
    Q_SIGNAL void scroll_factor_changed(double factor);
#endif
    explicit SciQLopPlot(QWidget* parent = nullptr) : QCustomPlot { parent }
    {
        using namespace Constants;
        this->addLayer(LayersNames::Spans, this->layer(LayersNames::Main), QCustomPlot::limAbove);
        this->layer(LayersNames::Spans)->setMode(QCPLayer::lmBuffered);
        this->layer(LayersNames::Spans)->setVisible(true);
        this->addLayer(
            LayersNames::SpansBorders, this->layer(LayersNames::Spans), QCustomPlot::limAbove);
        this->layer(LayersNames::SpansBorders)->setMode(QCPLayer::lmBuffered);
        this->layer(LayersNames::SpansBorders)->setVisible(true);
        this->setFocusPolicy(Qt::StrongFocus);
    }

    virtual ~SciQLopPlot() Q_DECL_OVERRIDE { }
    inline QCPColorMap* addColorMap(QCPAxis* x, QCPAxis* y)
    {
        auto cm = new QCPColorMap(x, y);
        return cm;
    }

    inline SciQLopGraph* addSciQLopGraph(QCPAxis* x, QCPAxis* y, QStringList labels,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        auto sg = new SciQLopGraph(this, x, y, labels, dataOrder);
        return sg;
    }

    inline SciQLopGraph* addSciQLopGraph(
        QCPAxis* x, QCPAxis* y, SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        auto sg = new SciQLopGraph(this, x, y, dataOrder);
        return sg;
    }

    inline SciQLopCurve* addSciQLopCurve(QCPAxis* x, QCPAxis* y, QStringList labels,
        SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        auto sg = new SciQLopCurve(this, x, y, labels, dataOrder);
        return sg;
    }

    inline SciQLopCurve* addSciQLopCurve(
        QCPAxis* x, QCPAxis* y, SciQLopGraph::DataOrder dataOrder = SciQLopGraph::DataOrder::xFirst)
    {
        auto sg = new SciQLopCurve(this, x, y, dataOrder);
        return sg;
    }

    inline SciQLopColorMap* addSciQLopColorMap(QCPAxis* x, QCPAxis* y, const QString& name,
        SciQLopColorMap::DataOrder dataOrder = SciQLopColorMap::DataOrder::xFirst)
    {
        auto sg = new SciQLopColorMap(this, x, y, name, dataOrder);
        return sg;
    }

    void set_scroll_factor(double factor) noexcept;
    inline double scroll_factor() const noexcept { return m_scroll_factor; }

protected:
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;

    virtual void keyPressEvent(QKeyEvent* event) override;

    virtual bool event(QEvent* event) override;

private:
    void _wheel_pan(QCPAxis* axis, const double wheelSteps, const QPointF& pos);
    void _wheel_zoom(QCPAxis* axis, const double wheelSteps, const QPointF& pos);
};

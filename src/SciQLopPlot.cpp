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
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotItem.hpp"
#include "SciQLopPlots/constants.hpp"
#include <QGestureRecognizer>
#include <QSharedPointer>
#include <algorithm>
#include <cpp_utils/containers/algorithms.hpp>
#include <type_traits>

template <typename T>
inline void _set_selected(T* plottable, bool selected)
{
    if (plottable->selected() != selected)
    {
        if (selected)
            plottable->setSelection(QCPDataSelection(plottable->data()->dataRange()));
        else
            plottable->setSelection(QCPDataSelection());
    }
}

inline void _set_selected(QCPAbstractPlottable* plottable, bool selected)
{
    if (auto graph = dynamic_cast<QCPGraph*>(plottable); graph != nullptr)
    {
        _set_selected(graph, selected);
    }
    else if (auto curve = dynamic_cast<QCPCurve*>(plottable); curve != nullptr)
    {
        _set_selected(curve, selected);
    }
}

SciQLopPlot::SciQLopPlot(QWidget* parent) : QCustomPlot { parent }
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
    this->grabGesture(Qt::PinchGesture);
    this->grabGesture(Qt::PanGesture);

    this->m_tracer = new TracerWithToolTip(this);
    this->setMouseTracking(true);

    connect(this, &QCustomPlot::legendDoubleClick, this, &SciQLopPlot::_legend_double_clicked);
}

SciQLopPlot::~SciQLopPlot()
{
    delete m_tracer;
}

QCPColorMap* SciQLopPlot::addColorMap(QCPAxis* x, QCPAxis* y)
{
    auto cm = new QCPColorMap(x, y);
    return cm;
}

SciQLopGraph* SciQLopPlot::addSciQLopGraph(
    QCPAxis* x, QCPAxis* y, QStringList labels, SciQLopGraph::DataOrder dataOrder)
{
    return this->_new_plottable_wrapper<SciQLopGraph>(x, y, labels, dataOrder);
}

SciQLopGraph* SciQLopPlot::addSciQLopGraph(
    QCPAxis* x, QCPAxis* y, SciQLopGraph::DataOrder dataOrder)
{
    return this->_new_plottable_wrapper<SciQLopGraph>(x, y, dataOrder);
}

SciQLopCurve* SciQLopPlot::addSciQLopCurve(
    QCPAxis* x, QCPAxis* y, QStringList labels, SciQLopGraph::DataOrder dataOrder)
{
    return this->_new_plottable_wrapper<SciQLopCurve>(x, y, labels, dataOrder);
}

SciQLopCurve* SciQLopPlot::addSciQLopCurve(
    QCPAxis* x, QCPAxis* y, SciQLopGraph::DataOrder dataOrder)
{
    return this->_new_plottable_wrapper<SciQLopCurve>(x, y, dataOrder);
}

SciQLopColorMap* SciQLopPlot::addSciQLopColorMap(
    QCPAxis* x, QCPAxis* y, const QString& name, SciQLopColorMap::DataOrder dataOrder)
{
    return this->_new_plottable_wrapper<SciQLopColorMap>(x, y, name, dataOrder);
}

void SciQLopPlot::set_scroll_factor(double factor) noexcept
{
    m_scroll_factor = factor;
    Q_EMIT scroll_factor_changed(factor);
}

void SciQLopPlot::enable_cursor(bool enable) noexcept { }

void SciQLopPlot::enable_legend(bool show) noexcept
{
    this->legend->setVisible(show);
    this->legend->setSelectableParts(QCPLegend::spItems);
    this->replot(rpQueuedReplot);
}

void SciQLopPlot::minimize_margins()
{
    plotLayout()->setMargins(QMargins(0, 0, 0, 0));
    plotLayout()->setRowSpacing(0);
    for (auto rect : axisRects())
        rect->setMargins(QMargins(0, 0, 0, 0));

    setContentsMargins(0, 0, 0, 0);
    if (auto l = layout(); l != nullptr)
    {
        l->setSpacing(0);
        l->setContentsMargins(0, 0, 0, 0);
    }
}

void SciQLopPlot::mousePressEvent(QMouseEvent* event)
{
    QCustomPlot::mousePressEvent(event);
}

void SciQLopPlot::mouseMoveEvent(QMouseEvent* event)
{
    QCustomPlot::mouseMoveEvent(event);
    _update_mouse_cursor(event);
    _update_tracer(event->pos());
    QWidget::mouseMoveEvent(event);
}

void SciQLopPlot::enterEvent(QEnterEvent* event)
{
    event->accept();
    QCustomPlot::enterEvent(event);
}

void SciQLopPlot::leaveEvent(QEvent* event)
{
    m_tracer->set_plotable(nullptr);
    event->accept();
    QCustomPlot::leaveEvent(event);
}

void SciQLopPlot::mouseReleaseEvent(QMouseEvent* event)
{
    QCustomPlot::mouseReleaseEvent(event);
}

void SciQLopPlot::_wheel_zoom(QCPAxis* axis, const double wheelSteps, const QPointF& pos)
{
    const auto factor = qPow(axis->axisRect()->rangeZoomFactor(axis->orientation()), wheelSteps);
    axis->scaleRange(
        factor, axis->pixelToCoord(axis->orientation() == Qt::Horizontal ? pos.x() : pos.y()));
}

void SciQLopPlot::_wheel_pan(QCPAxis* axis, const double wheelSteps, const QPointF& pos)
{
    axis->moveRange(wheelSteps * m_scroll_factor * QApplication::wheelScrollLines() / 100.
        * axis->range().size());
}

void SciQLopPlot::wheelEvent(QWheelEvent* event)
{
    const auto pos = event->position();
    const auto wheelSteps = event->angleDelta().y() / 120.0;
    foreach (QCPLayerable* candidate, layerableListAt(pos, false))
    {
        if (auto axis = qobject_cast<QCPAxis*>(candidate); axis != nullptr)
        {
            this->_wheel_zoom(axis, wheelSteps, pos);
            event->accept();
            this->replot(rpQueuedReplot);
            return;
        }
        else if (auto axisRect = qobject_cast<QCPAxisRect*>(candidate); axisRect != nullptr)
        {
            if (event->modifiers().testFlags(Qt::ShiftModifier | Qt::ControlModifier))
                this->_wheel_pan(axisRect->axis(QCPAxis::atLeft), wheelSteps, pos);
            else if (event->modifiers().testFlag(Qt::ControlModifier))
                this->_wheel_zoom(axisRect->axis(QCPAxis::atBottom), wheelSteps, pos);
            else if (event->modifiers().testFlag(Qt::ShiftModifier))
                this->_wheel_zoom(axisRect->axis(QCPAxis::atLeft), wheelSteps, pos);
            else
                this->_wheel_pan(axisRect->axis(QCPAxis::atBottom), wheelSteps, pos);
            this->replot(rpQueuedReplot);
            event->accept();
            return;
        }
    }
    QCustomPlot::wheelEvent(event);
}

void SciQLopPlot::keyPressEvent(QKeyEvent* event)
{
    auto items = selectedItems();
    event->setAccepted(false);
    std::for_each(items.begin(), items.end(),
        [event, this](auto item)
        {
            if (auto sciItem = dynamic_cast<SciQLopItemWithKeyInteraction*>(item);
                sciItem != nullptr)
            {
                sciItem->keyPressEvent(event);
            }
            else if (auto axis = dynamic_cast<QCPAxis*>(item); axis != nullptr)
            {
                if (axis->orientation() == Qt::Vertical)
                {
                    switch (event->key())
                    {
                        case Qt::Key_L:
                        {
                            if (axis->scaleType() == QCPAxis::stLinear)
                            {
                                axis->setScaleType(QCPAxis::stLogarithmic);
                                axis->setTicker(QSharedPointer<QCPAxisTickerLog>());
                            }
                            else
                            {
                                axis->setScaleType(QCPAxis::stLinear);
                                axis->setTicker(QSharedPointer<QCPAxisTicker>());
                            }
                            event->accept();
                            this->replot(rpQueuedReplot);
                        }
                        case Qt::Key_M:
                        {
                            axis->rescale(true);
                            event->accept();
                            this->replot(rpQueuedReplot);
                        }
                    }
                }
            }
        });
    if (event->isAccepted() == false)
    {
        switch (event->key())
        {
            case Qt::Key_L:
            {
                if (axisRect()->axis(QCPAxis::atLeft)->scaleType() == QCPAxis::stLinear)
                    axisRect()->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
                else
                    axisRect()->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLinear);
                event->accept();
                this->replot(rpQueuedReplot);
            }
            case Qt::Key_M:
            {
                axisRect()->axis(QCPAxis::atLeft)->rescale(true);
                if (auto a = axisRect()->axis(QCPAxis::atRight); a != nullptr)
                    a->rescale(true);
                event->accept();
                this->replot(rpQueuedReplot);
            }
        }
    }
    if (event->isAccepted() == false)
        QCustomPlot::keyPressEvent(event);
}


bool SciQLopPlot::event(QEvent* event)
{
    auto r = QCustomPlot::event(event);
    if (event->type() == QEvent::ToolTip)
    {
        return this->_handle_tool_tip(event);
    }
    if (event->type() == QEvent::Gesture)
    {
        QGestureEvent* gestureEvent = static_cast<QGestureEvent*>(event);
        if (QGesture* pan = gestureEvent->gesture(Qt::PanGesture); pan != nullptr)
        {
            if (auto p = dynamic_cast<QPanGesture*>(pan); p != nullptr)
            {
                event->accept();
            }
        }
        if (QGesture* pinch = gestureEvent->gesture(Qt::PinchGesture); pinch != nullptr)
        {
            if (auto p = dynamic_cast<QPinchGesture*>(pinch); p != nullptr)
            {
                event->accept();
            }
        }
        return true;
    }
    return r;
}

bool SciQLopPlot::_update_tracer(const QPointF& pos)
{
    auto plotable = plottableAt(pos, false);

    if (plotable != nullptr and plotable->visible()
        && (!legend->visible() || -1. == legend->selectTest(pos, false)))
    {
        m_tracer->set_plotable(plotable);
        m_tracer->update_position(pos);
        return true;
    }
    else
    {
        m_tracer->set_plotable(nullptr);
        return false;
    }
}

bool SciQLopPlot::_update_mouse_cursor(QMouseEvent* event)
{
    const auto item = itemAt(event->pos(), false);
    if (auto sciItem = dynamic_cast<SciQLopPlotItemBase*>(item); sciItem != nullptr)
    {
        this->setCursor(sciItem->cursor(event));
        return true;
    }
    this->setCursor(Qt::ArrowCursor);
    return false;
}

bool SciQLopPlot::_handle_tool_tip(QEvent* event)
{
    QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
    {
        auto itm = itemAt(helpEvent->pos(), false);
        if (itm)
        {
            if (auto itm_with_tt = dynamic_cast<SciQlopItemWithToolTip*>(itm); itm_with_tt)
            {
                QToolTip::showText(helpEvent->globalPos(), itm_with_tt->tooltip());
                return true;
            }
        }
    }
    if (!m_tracer->visible())
        QToolTip::hideText();
    return true;
}

QCPGraph* SciQLopPlot::_nearest_graph(const QPointF& pos)
{
    return plottableAt<QCPGraph>(pos, false);
}

std::optional<std::tuple<double, double>> SciQLopPlot::_nearest_data_point(
    const QPointF& pos, QCPGraph* graph)
{
    QCPGraphDataContainer::const_iterator it = graph->data()->constEnd();
    QVariant details;
    if (graph->selectTest(pos, false, &details))
    {
        QCPDataSelection dataPoints = details.value<QCPDataSelection>();
        if (dataPoints.dataPointCount() > 0)
        {
            it = graph->data()->at(dataPoints.dataRange().begin());
            return std::make_tuple(it->key, it->value);
        }
    }
    return std::nullopt;
}

void SciQLopPlot::_register_plottable(QCPAbstractPlottable* plotable)
{
    std::cout << "Registering plottable" << std::endl;
    connect(plotable, QOverload<bool>::of(&QCPAbstractPlottable::selectionChanged), this,
        [this, plotable](bool selected)
        {
            auto item = this->legend->itemWithPlottable(plotable);
            if (item && item->selected() != selected)
                item->setSelected(selected);
        });

    if (auto legend_item = this->legend->itemWithPlottable(plotable); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
            [this, plotable](bool selected)
            {
                _set_selected(plotable, selected);
                this->replot(rpQueuedReplot);
            });
    }
}

QCPAbstractPlottable* SciQLopPlot::plottable(const QString& name) const
{
    for (auto plottable : mPlottables)
    {
        if (plottable->name() == name)
            return plottable;
    }
    return nullptr;
}

void SciQLopPlot::_legend_double_clicked(
    QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event)
{
    if (auto legend_item = dynamic_cast<QCPPlottableLegendItem*>(item); legend_item != nullptr)
    {
        auto plottable = legend_item->plottable();
        plottable->setVisible(!plottable->visible());
        if (plottable->visible())
        {
            item->setTextColor(Qt::black);
            item->setSelectedTextColor(Qt::black);
        }
        else
        {
            item->setTextColor(Qt::gray);
            item->setSelectedTextColor(Qt::gray);
        }
        this->replot(rpQueuedReplot);
    }
}

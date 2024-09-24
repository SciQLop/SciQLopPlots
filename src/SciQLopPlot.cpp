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
#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Inspector/Model/Model.hpp"
#include "SciQLopPlots/Items/SciQLopPlotItem.hpp"
#include "SciQLopPlots/constants.hpp"

#include <QGestureRecognizer>
#include <QSharedPointer>
#include <algorithm>
#include <cpp_utils/containers/algorithms.hpp>
#include <utility>

// template <typename T>
// inline void _set_selected(T* plottable, bool selected)
// {
//     if (plottable->selected() != selected)
//     {
//         if (selected)
//             plottable->setSelection(QCPDataSelection(plottable->data()->dataRange()));
//         else
//             plottable->setSelection(QCPDataSelection());
//     }
// }

// inline void _set_selected(QCPAbstractPlottable* plottable, bool selected)
// {
//     if (auto graph = dynamic_cast<QCPGraph*>(plottable); graph != nullptr)
//     {
//         _set_selected(graph, selected);
//     }
//     else if (auto curve = dynamic_cast<QCPCurve*>(plottable); curve != nullptr)
//     {
//         _set_selected(curve, selected);
//     }
// }

namespace _impl
{


SciQLopPlot::SciQLopPlot(QWidget* parent) : QCustomPlot { parent }
{
    this->m_replot_timer = new QTimer(this);
    this->m_replot_timer->setSingleShot(true);
    this->m_replot_timer->setInterval(10);
    connect(this->m_replot_timer, &QTimer::timeout, this,
        [this]() { QCustomPlot::replot(rpQueuedReplot); });
    connect(this, &QCustomPlot::afterReplot, this, [this]() { m_replot_pending = false; });
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
    this->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables
        | QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectItems | QCP::iMultiSelect);


    connect(this, &QCustomPlot::legendDoubleClick, this, &SciQLopPlot::_legend_double_clicked);
    connect(this->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        [this](const QCPRange& range)
        { Q_EMIT x_axis_range_changed({ range.lower, range.upper }); });
    connect(this->xAxis2, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        [this](const QCPRange& range)
        { Q_EMIT x2_axis_range_changed({ range.lower, range.upper }); });
    connect(this->yAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        [this](const QCPRange& range)
        { Q_EMIT y_axis_range_changed({ range.lower, range.upper }); });
    connect(this->yAxis2, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
        [this](const QCPRange& range)
        { Q_EMIT y2_axis_range_changed({ range.lower, range.upper }); });

    m_color_scale = new QCPColorScale(this);
    m_color_scale->setVisible(false);

    this->m_axes.append(new SciQLopPlotAxis(this->xAxis, this));
    this->m_axes.append(new SciQLopPlotAxis(this->yAxis, this));
    this->m_axes.append(new SciQLopPlotAxis(this->xAxis2, this));
    this->m_axes.append(new SciQLopPlotAxis(this->yAxis2, this));
    this->m_axes.append(new SciQLopPlotColorScaleAxis(this->m_color_scale, this));

#ifdef QCUSTOMPLOT_USE_OPENGL
    setOpenGl(true, 4);
#endif
    for (auto gesture : { Qt::PanGesture, Qt::PinchGesture })
    {
        grabGesture(gesture);
    }
}

SciQLopPlot::~SciQLopPlot()
{
    for (auto plottable : m_plottables)
    {
        disconnect(plottable, nullptr, this, nullptr);
    }
    m_plottables.clear();
    delete m_tracer;
}

QCPColorMap* SciQLopPlot::addColorMap()
{
    auto cm = new QCPColorMap(this->xAxis, this->yAxis2);
    return cm;
}

SQPQCPAbstractPlottableWrapper* SciQLopPlot::sqp_plottable(int index)
{
    if (index == -1 || index >= std::size(m_plottables))
        index = std::size(m_plottables) - 1;
    return m_plottables[index];
}

SQPQCPAbstractPlottableWrapper* SciQLopPlot::sqp_plottable(const QString& name)
{
    for (auto p : m_plottables)
    {
        if (p->objectName() == name)
            return p;
    }
    return nullptr;
}

const QList<SQPQCPAbstractPlottableWrapper*>& SciQLopPlot::sqp_plottables() const
{
    return m_plottables;
}

SciQLopColorMap* SciQLopPlot::add_color_map(const QString& name, bool y_log_scale, bool z_log_scale)
{
    if (!m_color_scale->visible())
    {

        auto cmap = this->_new_plottable_wrapper<SciQLopColorMap>(this->xAxis, this->yAxis2, name);
        _configure_color_map(cmap, y_log_scale, z_log_scale);
        return cmap;
    }
    return nullptr;
}

SciQLopColorMapFunction* SciQLopPlot::add_color_map(
    GetDataPyCallable&& callable, const QString& name, bool y_log_scale, bool z_log_scale)
{
    if (!m_color_scale->visible())
    {

        auto cmap = this->_new_plottable_wrapper<SciQLopColorMapFunction>(
            this->xAxis, this->yAxis2, std::move(callable), name);
        _configure_color_map(cmap, y_log_scale, z_log_scale);
        return cmap;
    }
    return nullptr;
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

QMargins SciQLopPlot::minimal_axis_margins()
{
    return QMargins(_minimal_margin(QCP::msLeft), _minimal_margin(QCP::msTop),
        _minimal_margin(QCP::msRight), _minimal_margin(QCP::msBottom));
}

void SciQLopPlot::replot(RefreshPriority priority)
{
    if (priority == rpImmediateRefresh)
    {
        QCustomPlot::replot(rpImmediateRefresh);
    }
    else
    {
        if (!m_replot_timer->isActive() && !m_replot_pending)
        {
            m_replot_pending = true;
            m_replot_timer->start();
        }
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

int SciQLopPlot::_minimal_margin(QCP::MarginSide side)
{
    return 0;
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
    std::for_each(items.begin(), items.end(),
        [event](auto item)
        {
            if (auto sciItem = dynamic_cast<SciQLopItemWithKeyInteraction*>(item);
                sciItem != nullptr)
            {
                sciItem->keyPressEvent(event);
            }
        });
    if (event->key() == Qt::Key_Escape)
    {
        for (auto item : items)
        {
            item->setSelected(false);
        }
        for (auto g : selectedGraphs())
        {
            g->setSelection(QCPDataSelection());
        }
        for (auto l : selectedLegends())
        {
            l->setSelectedParts(QCPLegend::SelectablePart::spNone);
        }
        for (auto a : selectedPlottables())
        {
            a->setSelection(QCPDataSelection());
        }
        for (auto ax : selectedAxes())
        {
            ax->setSelectedParts(QCPAxis::spNone);
        }
        this->replot(rpQueuedReplot);
    }
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

void SciQLopPlot::_register_plottable_wrapper(SQPQCPAbstractPlottableWrapper* plottable)
{
    for (auto qcp_plottable : plottable->qcp_plottables())
    {
        _register_plottable(qcp_plottable);
    }
    m_plottables.append(plottable);
    connect(plottable, &SQPQCPAbstractPlottableWrapper::plottable_created, this,
        &SciQLopPlot::_register_plottable);
    connect(plottable, &SQPQCPAbstractPlottableWrapper::destroyed, this,
        [this, plottable]()
        {
            m_plottables.removeOne(plottable);
            emit this->plotables_list_changed();
        });
    connect(plottable, &SQPQCPAbstractPlottableWrapper::replot, this, [this]() { this->replot(); });
    emit this->plotables_list_changed();
}

void SciQLopPlot::_register_plottable(QCPAbstractPlottable* plotable)
{
    connect(plotable, QOverload<bool>::of(&QCPAbstractPlottable::selectionChanged), this,
        [this, plotable](bool selected)
        {
            auto item = this->legend->itemWithPlottable(plotable);
            if (item && item->selected() != selected)
            {
                item->setSelected(selected);
            }
            auto plotable_wrapper = this->plottable_wrapper(plotable);
            emit this->plottable_wrapper(plotable)->selection_changed(plotable_wrapper->selected());
        });

    if (auto legend_item = this->legend->itemWithPlottable(plotable); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
            [this, plotable](bool selected)
            {
                set_selected(plotable, selected);
                this->replot(rpQueuedReplot);
            });
    }
}

SQPQCPAbstractPlottableWrapper* SciQLopPlot::plottable_wrapper(QCPAbstractPlottable* plottable)
{
    for (auto p : m_plottables)
    {
        if (p->qcp_plottables().contains(plottable))
            return p;
    }
    return nullptr;
}

void SciQLopPlot::_configure_color_map(SciQLopColorMap* cmap, bool y_log_scale, bool z_log_scale)
{
    if (!m_color_scale->visible())
    {
        if (y_log_scale)
        {
            set_axis_log(QCPAxis::atRight, true);
        }
        set_axis_visible(QCPAxis::atRight, true);
        m_color_scale->setVisible(true);
        plotLayout()->addElement(0, 1, m_color_scale);
        cmap->colorMap()->setColorScale(m_color_scale);
        cmap->colorMap()->setInterpolate(false);
        if (z_log_scale)
        {
            cmap->colorMap()->setDataScaleType(QCPAxis::stLogarithmic);
            m_color_scale->setDataScaleType(QCPAxis::stLogarithmic);
            m_color_scale->axis()->setTicker(
                QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
        }

        auto gradient = QCPColorGradient(QCPColorGradient::gpJet);
        gradient.setNanHandling(QCPColorGradient::nhTransparent);
        cmap->colorMap()->setGradient(gradient);
        cmap->set_auto_scale_y(true);
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
}

void SciQLopPlot::_configure_plotable(SQPQCPAbstractPlottableWrapper* plottable,
    const QStringList& labels, const QList<QColor>& colors)
{
    if (plottable)
    {
        if (std::size(colors) <= plottable->plottable_count())
        {
            for (std::size_t i = 0; i < std::size(colors); i++)
            {
                plottable->qcp_plottables()[i]->setPen(QPen(colors[i]));
            }
        }
    }
}

SciQLopPlot::SciQLopPlot(QWidget* parent) : SciQLopPlotInterface(parent)
{
    m_impl = new _impl::SciQLopPlot(this);
    this->m_time_axis = new SciQLopPlotDummyAxis(this);
    connect(this->m_time_axis, &SciQLopPlotDummyAxis::range_changed, this,
        &SciQLopPlot::time_axis_range_changed);

    // connect(m_impl, &_impl::SciQLopPlot::x2_axis_range_changed, this,
    //    &SciQLopPlot::time_axis_range_changed);

    this->setLayout(new QVBoxLayout);
    this->layout()->addWidget(m_impl);
    connect(m_impl, &_impl::SciQLopPlot::x_axis_range_changed, this,
        &SciQLopPlot::x_axis_range_changed);
    connect(m_impl, &_impl::SciQLopPlot::x2_axis_range_changed, this,
        &SciQLopPlot::x2_axis_range_changed);
    connect(m_impl, &_impl::SciQLopPlot::y_axis_range_changed, this,
        &SciQLopPlot::y_axis_range_changed);
    connect(m_impl, &_impl::SciQLopPlot::y2_axis_range_changed, this,
        &SciQLopPlot::y2_axis_range_changed);
    connect(m_impl, &_impl::SciQLopPlot::scroll_factor_changed, this,
        &SciQLopPlot::scroll_factor_changed);

    connect(m_impl, &_impl::SciQLopPlot::plotables_list_changed, this,
        &SciQLopPlot::graph_list_changed);

    set_axes_to_rescale(
        QList<SciQLopPlotAxisInterface*> { x_axis(), x2_axis(), y_axis(), y2_axis(), z_axis() });
    this->enable_legend(true);
    this->minimize_margins();
}

SciQLopPlot::~SciQLopPlot() { }

void SciQLopPlot::set_scroll_factor(double factor) noexcept
{
    m_impl->set_scroll_factor(factor);
    Q_EMIT scroll_factor_changed(factor);
}

double SciQLopPlot::scroll_factor() const noexcept
{
    return m_impl->scroll_factor();
}

void SciQLopPlot::enable_cursor(bool enable) noexcept { }

void SciQLopPlot::enable_legend(bool show) noexcept
{
    m_impl->legend->setVisible(show);
    m_impl->legend->setSelectableParts(QCPLegend::spItems);
    replot();
}

void SciQLopPlot::minimize_margins()
{
    m_impl->minimize_margins();
    setContentsMargins(0, 0, 0, 0);
    if (auto l = layout(); l != nullptr)
    {
        l->setSpacing(0);
        l->setContentsMargins(0, 0, 0, 0);
    }
}

void SciQLopPlot::replot(bool immediate)
{
    if (immediate)
        m_impl->replot();
    else
        m_impl->replot(QCustomPlot::rpQueuedReplot);
}

SciQLopGraphInterface* SciQLopPlot::plot_impl(const PyBuffer& x, const PyBuffer& y,
    QStringList labels, QList<QColor> colors, GraphType graph_type)
{
    SQPQCPAbstractPlottableWrapper* plottable = nullptr;
    switch (graph_type)
    {
        case GraphType::Line:
            plottable = m_impl->add_plottable<SciQLopLineGraph>(labels);
            break;
        case GraphType::ParametricCurve:
            plottable = m_impl->add_plottable<SciQLopCurve>(labels);
            break;
        default:
            throw std::runtime_error("Unsupported graph type");
            break;
    }
    if (plottable)
    {
        plottable->set_data(std::move(x), std::move(y));
        _configure_plotable(plottable, labels, colors);
    }
    return plottable;
}

SciQLopGraphInterface* SciQLopPlot::plot_impl(const PyBuffer& x, const PyBuffer& y,
    const PyBuffer& z, QString name, bool y_log_scale, bool z_log_scale)
{
    auto cm = m_impl->add_color_map(name, y_log_scale, z_log_scale);
    cm->set_data(std::move(x), std::move(y), std::move(z));
    return cm;
}

void SciQLopPlot::_connect_callable_sync(
    SQPQCPAbstractPlottableWrapper* plottable, QObject* sync_with)
{
    if (sync_with != nullptr)
    {
        if (auto axis = qobject_cast<SciQLopPlotAxisInterface*>(sync_with); axis != nullptr)
        {
            connect(axis, &SciQLopPlotAxisInterface::range_changed, plottable,
                &SQPQCPAbstractPlottableWrapper::set_range);
        }
        if (auto graph = qobject_cast<SciQLopGraphInterface*>(sync_with); graph != nullptr)
        {
            connect(graph, QOverload<PyBuffer, PyBuffer>::of(&SciQLopGraphInterface::data_changed),
                plottable,
                QOverload<PyBuffer, PyBuffer>::of(&SQPQCPAbstractPlottableWrapper::set_data),
                Qt::QueuedConnection);

            connect(graph,
                QOverload<PyBuffer, PyBuffer, PyBuffer>::of(&SciQLopGraphInterface::data_changed),
                plottable,
                QOverload<PyBuffer, PyBuffer, PyBuffer>::of(
                    &SQPQCPAbstractPlottableWrapper::set_data),
                Qt::QueuedConnection);
        }
    }
    else
    {
        connect(this->x_axis(), &SciQLopPlotAxisInterface::range_changed, plottable,
            &SQPQCPAbstractPlottableWrapper::set_range);
    }
}

SciQLopGraphInterface* SciQLopPlot::plot_impl(GetDataPyCallable callable, QStringList labels,
    QList<QColor> colors, GraphType graph_type, QObject* sync_with)
{
    SQPQCPAbstractPlottableWrapper* plottable = nullptr;
    switch (graph_type)
    {
        case GraphType::Line:
            plottable
                = m_impl->add_plottable<SciQLopLineGraphFunction>(std::move(callable), labels);
            break;
        case GraphType::ParametricCurve:
            plottable = m_impl->add_plottable<SciQLopCurveFunction>(std::move(callable), labels);
            break;
        default:
            break;
    }
    if (plottable)
    {
        _connect_callable_sync(plottable, sync_with);
        _configure_plotable(plottable, labels, colors);
    }
    return plottable;
}

SciQLopGraphInterface* SciQLopPlot::plot_impl(GetDataPyCallable callable, QString name,
    bool y_log_scale, bool z_log_scale, QObject* sync_with)
{
    SQPQCPAbstractPlottableWrapper* plotable = nullptr;
    plotable = m_impl->add_color_map(std::move(callable), name, y_log_scale, z_log_scale);
    if (plotable)
    {
        _connect_callable_sync(plotable, sync_with);
    }
    return plotable;
}

SciQLopGraphInterface* SciQLopPlot::graph(int index)
{
    return m_impl->sqp_plottable(index);
}

SciQLopGraphInterface* SciQLopPlot::graph(const QString& name)
{
    return m_impl->sqp_plottable(name);
}

QList<SciQLopGraphInterface*> SciQLopPlot::graphs() const noexcept
{
    QList<SciQLopGraphInterface*> graphs;
    for (auto p : m_impl->sqp_plottables())
    {
        graphs.append(p);
    }
    return graphs;
}

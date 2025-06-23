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

namespace _impl
{


SciQLopPlot::SciQLopPlot(QWidget* parent) : QCustomPlot { parent }
{
    using namespace Constants;
    setAttribute(Qt::WA_OpaquePaintEvent);
    this->m_replot_timer = new QTimer(this);
    this->m_replot_timer->setSingleShot(true);
    this->m_replot_timer->setTimerType(Qt::PreciseTimer);
    this->m_replot_timer->setInterval(5);
    this->connect(this, &QCustomPlot::beforeReplot, this, [this]() { m_replot_timer->start(); });
    connect(this->m_replot_timer, &QTimer::timeout, this,
            [this]()
            {
                if (m_replot_pending)
                {
                    m_replot_pending = false;
                    QCustomPlot::replot(rpQueuedReplot);
                }
            });
    this->addLayer(LayersNames::Spans, this->layer(LayersNames::Main), QCustomPlot::limAbove);
    this->layer(LayersNames::Spans)->setMode(QCPLayer::lmBuffered);
    this->layer(LayersNames::Spans)->setVisible(true);
    this->addLayer(LayersNames::SpansBorders, this->layer(LayersNames::Spans),
                   QCustomPlot::limAbove);
    this->layer(LayersNames::SpansBorders)->setMode(QCPLayer::lmBuffered);
    this->layer(LayersNames::SpansBorders)->setVisible(true);
    this->addLayer(LayersNames::ColorMap, this->layer(LayersNames::Main), QCustomPlot::limBelow);
    this->layer(LayersNames::ColorMap)->setMode(QCPLayer::lmBuffered);
    this->layer(LayersNames::ColorMap)->setVisible(true);
    this->setFocusPolicy(Qt::StrongFocus);
    this->grabGesture(Qt::PinchGesture, Qt::DontStartGestureOnChildren);
    this->grabGesture(Qt::PanGesture, Qt::DontStartGestureOnChildren);

    this->m_tracer = new TracerWithToolTip(this);
    this->setMouseTracking(true);
    this->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables
                          | QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectItems
                          | QCP::iMultiSelect);


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
    m_color_scale->axis()->setNumberFormat("gb");

    m_color_scale->axis()->setNumberPrecision(0);

    connect(m_color_scale->axis(), QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
            [this](const QCPRange& range)
            { Q_EMIT z_axis_range_changed({ range.lower, range.upper }); });

    this->m_axes.append(new SciQLopPlotAxis(this->xAxis, this, false, "X Axis"));
    this->m_axes.append(new SciQLopPlotAxis(this->yAxis, this, false, "Y Axis"));
    this->m_axes.append(new SciQLopPlotAxis(this->xAxis2, this, false, "X Axis 2"));
    this->m_axes.append(new SciQLopPlotAxis(this->yAxis2, this, false, "Y Axis 2"));
    this->m_axes.append(new SciQLopPlotColorScaleAxis(this->m_color_scale, this, "Color Scale"));

#ifdef QCUSTOMPLOT_USE_OPENGL
    setOpenGl(true, 4);
#endif
    for (auto gesture : { Qt::PinchGesture })
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
    using namespace Constants;
    auto cm = new QCPColorMap(this->xAxis, this->yAxis2);
    cm->setLayer(LayersNames::ColorMap);
    return cm;
}

SciQLopPlottableInterface* SciQLopPlot::sqp_plottable(int index)
{
    if (index == -1 || index >= std::size(m_plottables))
        index = std::size(m_plottables) - 1;
    return m_plottables[index];
}

SciQLopPlottableInterface* SciQLopPlot::sqp_plottable(const QString& name)
{
    for (auto p : m_plottables)
    {
        if (p->objectName() == name)
            return p;
    }
    return nullptr;
}

const QList<SciQLopPlottableInterface*>& SciQLopPlot::sqp_plottables() const
{
    return m_plottables;
}

SciQLopColorMap* SciQLopPlot::add_color_map(const QString& name, bool y_log_scale, bool z_log_scale)
{
    if (m_color_map == nullptr)
    {
        m_color_map = new SciQLopColorMap(
            this, this->m_axes[0], this->m_axes[3],
            reinterpret_cast<SciQLopPlotColorScaleAxis*>(this->m_axes[4]), name);
        _ensure_colorscale_is_visible(m_color_map);
        _register_plottable_wrapper(m_color_map);
        return m_color_map;
    }
    return nullptr;
}

SciQLopColorMapFunction* SciQLopPlot::add_color_map(GetDataPyCallable&& callable,
                                                    const QString& name, bool y_log_scale,
                                                    bool z_log_scale)
{
    if (m_color_map == nullptr)
    {
        m_color_map = new SciQLopColorMapFunction(
            this, this->m_axes[0], this->m_axes[3],
            reinterpret_cast<SciQLopPlotColorScaleAxis*>(this->m_axes[4]), std::move(callable),
            name);
        _ensure_colorscale_is_visible(m_color_map);
        _register_plottable_wrapper(m_color_map);
        return qobject_cast<SciQLopColorMapFunction*>(m_color_map);
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
        if (!m_replot_pending && m_replot_timer->isActive())
        {
            m_replot_pending = true;
        }
        else
        {
            QCustomPlot::replot(rpQueuedReplot);
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
    axis->scaleRange(factor,
                     axis->pixelToCoord(axis->orientation() == Qt::Horizontal ? pos.x() : pos.y()));
}

void SciQLopPlot::_pinch_zoom(QPinchGesture* gesture)
{
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged)
    {
        auto axis = this->xAxis;
        if (gesture->scaleFactor() != 0.)
            axis->scaleRange(1. / gesture->scaleFactor(),
                             axis->pixelToCoord(gesture->centerPoint().x()));
    }
}

int SciQLopPlot::_minimal_margin(QCP::MarginSide side)
{
    return 0;
}

void SciQLopPlot::_wheel_pan(QCPAxis* axis, const double wheelSteps)
{
    if (axis->scaleType() == QCPAxis::stLinear)
    {
        axis->moveRange(wheelSteps * m_scroll_factor * QApplication::wheelScrollLines() / 100.
                        * axis->range().size());
    }
    else
    {
        auto size = log10(axis->range().upper) - log10(axis->range().lower);
        if (wheelSteps > 0)
            axis->moveRange(std::pow(
                10, wheelSteps * m_scroll_factor * QApplication::wheelScrollLines() / 100. * size));
        else
            axis->moveRange(1.
                            / std::pow(10,
                                       -wheelSteps * m_scroll_factor
                                           * QApplication::wheelScrollLines() / 100. * size));
    }
}

inline double _signed_length(const QPoint& p)
{
    auto sign = p.y() - p.x();
    return sign < 0 ? -p.manhattanLength() : p.manhattanLength();
}

void SciQLopPlot::wheelEvent(QWheelEvent* event)
{
    const auto pos = event->position();
    // Qt converts two finger scroll to wheel events, use manhattanLength to
    // ingore the direction of the scroll
    const auto inside_axis_rect = this->axisRect()->rect().contains(pos.toPoint());
    const auto wheelSteps = _signed_length(event->angleDelta()) / 120.0;
    for (auto candidate : layerableListAt(pos, false))
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
                this->_wheel_pan(axisRect->axis(QCPAxis::atLeft), wheelSteps);
            else if (event->modifiers().testFlag(Qt::ControlModifier))
                this->_wheel_zoom(axisRect->axis(QCPAxis::atBottom), wheelSteps, pos);
            else if (event->modifiers().testFlag(Qt::ShiftModifier))
                this->_wheel_zoom(axisRect->axis(QCPAxis::atLeft), wheelSteps, pos);
            else if (inside_axis_rect)
                this->_wheel_pan(axisRect->axis(QCPAxis::atBottom), wheelSteps);
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
                      if (auto sciItem = dynamic_cast<impl::SciQLopItemWithKeyInteraction*>(item);
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
        if (QGesture* gesture = gestureEvent->gesture(Qt::PinchGesture); gesture != nullptr)
        {
            if (auto p = dynamic_cast<QPinchGesture*>(gesture); p != nullptr)
            {
                _pinch_zoom(p);
                event->accept();
            }
        }
        return true;
    }
    return r;
}

void SciQLopPlot::resizeEvent(QResizeEvent* event)
{
    QCustomPlot::resizeEvent(event);
    Q_EMIT resized(event->size());
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
    if (auto sciItem = dynamic_cast<impl::SciQLopPlotItemBase*>(item); sciItem != nullptr)
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
            if (auto itm_with_tt = dynamic_cast<impl::SciQlopItemWithToolTip*>(itm); itm_with_tt)
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

std::optional<std::tuple<double, double>> SciQLopPlot::_nearest_data_point(const QPointF& pos,
                                                                           QCPGraph* graph)
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

void SciQLopPlot::_register_plottable_wrapper(SciQLopPlottableInterface* plottable)
{
    m_plottables.append(plottable);
    connect(plottable, &SciQLopGraphInterface::destroyed, this,
            [this, plottable]()
            {
                m_plottables.removeOne(plottable);
                emit this->plotables_list_changed();
            });
    connect(plottable, &SciQLopGraphInterface::replot, this, [this]() { this->replot(); });
    connect(this, &SciQLopPlot::resized, plottable,
            &SciQLopPlottableInterface::parent_plot_resized);
    emit this->plotables_list_changed();
}

void _impl::SciQLopPlot::_ensure_colorscale_is_visible(SciQLopColorMap* cmap)
{
    if (!m_color_scale->visible())
    {
        m_color_scale->setVisible(true);
        plotLayout()->addElement(0, 1, m_color_scale);
        cmap->colorMap()->setColorScale(m_color_scale);
        cmap->colorMap()->setInterpolate(false);
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

void SciQLopPlot::_legend_double_clicked(QCPLegend* legend, QCPAbstractLegendItem* item,
                                         QMouseEvent* event)
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

void SciQLopPlot::_configure_plotable(SciQLopGraphInterface* plottable, const QStringList& labels,
                                      const QList<QColor>& colors, ::GraphType graph_type,
                                      ::GraphMarkerShape marker)
{
    if (plottable)
    {
        if (std::size(colors) == std::size(plottable->components()))
        {
            plottable->set_colors(colors);
        }
        else
        {
            for (auto& component : plottable->components())
            {
                component->set_color(m_color_palette[m_color_palette_index]);
                component->set_marker_shape(marker);
                m_color_palette_index = (m_color_palette_index + 1) % std::size(m_color_palette);
                if (graph_type == ::GraphType::Scatter)
                {
                    component->set_line_style(::GraphLineStyle::NoLine);
                }
            }
        }
        if (std::size(labels) == std::size(plottable->components()))
            plottable->set_labels(labels);
        connect(
            plottable, QOverload<>::of(&SciQLopGraphInterface::data_changed), this,
            [this]()
            {
                if (this->m_auto_scale)
                    this->rescale_axes();
            },
            Qt::QueuedConnection);
        connect(
            plottable, &SciQLopGraphInterface::request_rescale, this,
            [this]() { this->rescale_axes(); }, Qt::QueuedConnection);
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
    connect(m_impl, &_impl::SciQLopPlot::z_axis_range_changed, this,
            &SciQLopPlot::z_axis_range_changed);

    connect(m_impl, &_impl::SciQLopPlot::scroll_factor_changed, this,
            &SciQLopPlot::scroll_factor_changed);

    connect(m_impl, &_impl::SciQLopPlot::plotables_list_changed, this,
            &SciQLopPlot::graph_list_changed);

    set_axes_to_rescale(
        QList<SciQLopPlotAxisInterface*> { x_axis(), x2_axis(), y_axis(), y2_axis(), z_axis() });
    this->m_legend = new SciQLopPlotLegend(m_impl->legend, this);
    m_legend->set_visible(true);
    this->minimize_margins();
}

SciQLopPlot::~SciQLopPlot()
{
    while (plottables().size() > 0)
    {
        delete plottable(0);
    }
}

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
                                              QStringList labels, QList<QColor> colors,
                                              GraphType graph_type, GraphMarkerShape marker,
                                              QVariantMap metaData)
{
    SQPQCPAbstractPlottableWrapper* plottable = nullptr;
    switch (graph_type)
    {
        case GraphType::Line:
        case GraphType::Scatter:
            plottable = m_impl->add_plottable<SciQLopLineGraph>(labels, metaData);
            break;
        case GraphType::ParametricCurve:
            plottable = m_impl->add_plottable<SciQLopCurve>(labels, metaData);
            break;
        default:
            throw std::runtime_error("Unsupported graph type");
            break;
    }
    if (plottable)
    {
        plottable->set_data(std::move(x), std::move(y));
        _configure_plotable(plottable, labels, colors, graph_type, marker);
    }
    return plottable;
}

SciQLopColorMapInterface* SciQLopPlot::plot_impl(const PyBuffer& x, const PyBuffer& y,
                                                 const PyBuffer& z, QString name, bool y_log_scale,
                                                 bool z_log_scale, QVariantMap metaData)
{
    auto cm = m_impl->add_color_map(name, y_log_scale, z_log_scale);
    cm->set_meta_data(metaData);
    cm->set_data(std::move(x), std::move(y), std::move(z));
    _configure_color_map(cm, y_log_scale, z_log_scale);
    return cm;
}

void SciQLopPlot::_configure_color_map(SciQLopColorMapInterface* cmap, bool y_log_scale,
                                       bool z_log_scale)
{
    if (cmap)
    {
        {
            auto y_axis = cmap->y_axis();
            y_axis->set_log(y_log_scale);
            y_axis->set_visible(true);
        }
        {
            if (auto z_axis = reinterpret_cast<SciQLopPlotColorScaleAxis*>(cmap->z_axis()))
            {
                z_axis->set_log(z_log_scale);
                z_axis->set_visible(true);
            }
        }
        cmap->set_gradient(ColorGradient::Jet);
        connect(
            cmap, &SciQLopGraphInterface::request_rescale, this, [this]() { this->rescale_axes(); },
            Qt::QueuedConnection);
        connect(
            cmap, QOverload<>::of(&SciQLopColorMapInterface::data_changed), this,
            [this]()
            {
                if (this->m_auto_scale)
                    this->rescale_axes();
            },
            Qt::QueuedConnection);
    }
}

void SciQLopPlot::_connect_callable_sync(SciQLopPlottableInterface* plottable, QObject* sync_with)
{
    if (SciQLopFunctionGraph* graph = dynamic_cast<SciQLopFunctionGraph*>(plottable))
    {
        if (sync_with != nullptr)
        {
            graph->observe(sync_with);
        }
        else
        {
            graph->observe(this->x_axis());
        }
    }
}

SciQLopGraphInterface* SciQLopPlot::plot_impl(GetDataPyCallable callable, QStringList labels,
                                              QList<QColor> colors, GraphType graph_type,
                                              GraphMarkerShape marker, QObject* sync_with,
                                              QVariantMap metaData)
{
    SQPQCPAbstractPlottableWrapper* plottable = nullptr;
    switch (graph_type)
    {
        case GraphType::Line:
        case GraphType::Scatter:
            plottable = m_impl->add_plottable<SciQLopLineGraphFunction>(std::move(callable), labels,
                                                                        metaData);
            break;
        case GraphType::ParametricCurve:
            plottable = m_impl->add_plottable<SciQLopCurveFunction>(std::move(callable), labels,
                                                                    metaData);
            break;
        default:
            break;
    }
    if (plottable)
    {
        _connect_callable_sync(plottable, sync_with);
        _configure_plotable(plottable, labels, colors, graph_type, marker);
    }
    return plottable;
}

SciQLopColorMapInterface* SciQLopPlot::plot_impl(GetDataPyCallable callable, QString name,
                                                 bool y_log_scale, bool z_log_scale,
                                                 QObject* sync_with, QVariantMap metaData)
{
    SciQLopColorMapInterface* plotable = nullptr;
    plotable = m_impl->add_color_map(std::move(callable), name, y_log_scale, z_log_scale);
    if (plotable)
    {
        plotable->set_meta_data(metaData);
        _configure_color_map(plotable, y_log_scale, z_log_scale);
        _connect_callable_sync(plotable, sync_with);
    }
    return plotable;
}

void SciQLopPlot::toggle_selected_objects_visibility() noexcept
{
    for (auto item : m_impl->selectedItems())
    {
        item->setVisible(!item->visible());
    }
    replot();
}

SciQLopPlottableInterface* SciQLopPlot::plottable(int index)
{
    return m_impl->sqp_plottable(index);
}

SciQLopPlottableInterface* SciQLopPlot::plottable(const QString& name)
{
    return m_impl->sqp_plottable(name);
}

QList<SciQLopPlottableInterface*> SciQLopPlot::plottables() const noexcept
{
    return m_impl->sqp_plottables();
}

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
#include "SciQLopPlots/SciQLopTheme.hpp"
#include "SciQLopPlots/Inspector/Model/Model.hpp"
#include "SciQLopPlots/Items/SciQLopPlotItem.hpp"
#include "SciQLopPlots/constants.hpp"
#include <layoutelements/layoutelement-legend-group.h>
#include <plottables/plottable-multigraph.h>
#include <theme.h>

#include <QFileInfo>
#include <cmath>
#include <cpp_utils/containers/algorithms.hpp>
#include <limits>
#include <utility>

namespace _impl
{


SciQLopPlot::SciQLopPlot(QWidget* parent) : QCustomPlot { parent }
{
    using namespace Constants;
    setAttribute(Qt::WA_OpaquePaintEvent);
    this->addLayer(LayersNames::Spans, this->layer(LayersNames::Main), QCustomPlot::limAbove);
    this->layer(LayersNames::Spans)->setMode(QCPLayer::lmBuffered);
    this->layer(LayersNames::Spans)->setVisible(true);
    this->addLayer(LayersNames::ColorMap, this->layer(LayersNames::Main), QCustomPlot::limBelow);
    this->layer(LayersNames::ColorMap)->setMode(QCPLayer::lmBuffered);
    this->layer(LayersNames::ColorMap)->setVisible(true);
    this->setFocusPolicy(Qt::StrongFocus);
    this->grabGesture(Qt::PinchGesture, Qt::DontStartGestureOnChildren);
    this->grabGesture(Qt::PanGesture, Qt::DontStartGestureOnChildren);

    m_crosshair = new SciQLopCrosshair(this);
    this->setMouseTracking(true);
    this->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables
                          | QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectItems
                          | QCP::iMultiSelect);


    connect(this, &QCustomPlot::legendDoubleClick, this, &SciQLopPlot::_legend_double_clicked);
    connect(this->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
            [this](const QCPRange& range)
            { if (!m_suppress_range_signals) Q_EMIT x_axis_range_changed({ range.lower, range.upper }); });
    connect(this->xAxis2, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
            [this](const QCPRange& range)
            { if (!m_suppress_range_signals) Q_EMIT x2_axis_range_changed({ range.lower, range.upper }); });
    connect(this->yAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
            [this](const QCPRange& range)
            { if (!m_suppress_range_signals) Q_EMIT y_axis_range_changed({ range.lower, range.upper }); });
    connect(this->yAxis2, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
            [this](const QCPRange& range)
            { if (!m_suppress_range_signals) Q_EMIT y2_axis_range_changed({ range.lower, range.upper }); });

    m_color_scale = new QCPColorScale(this);
    m_color_scale->setVisible(false);
    m_color_scale->axis()->setNumberFormat("gb");

    m_color_scale->axis()->setNumberPrecision(0);

    connect(m_color_scale->axis(), QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
            [this](const QCPRange& range)
            { if (!m_suppress_range_signals) Q_EMIT z_axis_range_changed({ range.lower, range.upper }); });

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
    delete m_crosshair;
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
    if (m_plottables.isEmpty())
        return nullptr;
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
            static_cast<SciQLopPlotColorScaleAxis*>(this->m_axes[4]), name);
        _ensure_colorscale_is_visible(m_color_map);
        _register_plottable_wrapper(m_color_map);
        return m_color_map;
    }
    return nullptr;
}

SciQLopHistogram2D* SciQLopPlot::add_histogram2d(const QString& name, int key_bins, int value_bins)
{
    auto hist = new SciQLopHistogram2D(
        this, this->m_axes[0], this->m_axes[1],
        static_cast<SciQLopPlotColorScaleAxis*>(this->m_axes[4]), name, key_bins, value_bins);
    _ensure_colorscale_is_visible(hist);
    _register_plottable_wrapper(hist);
    return hist;
}

SciQLopHistogram2DFunction* SciQLopPlot::add_histogram2d(GetDataPyCallable&& callable,
                                                          const QString& name, int key_bins,
                                                          int value_bins)
{
    auto hist = new SciQLopHistogram2DFunction(
        this, this->m_axes[0], this->m_axes[1],
        static_cast<SciQLopPlotColorScaleAxis*>(this->m_axes[4]), std::move(callable), name,
        key_bins, value_bins);
    _ensure_colorscale_is_visible(hist);
    _register_plottable_wrapper(hist);
    return hist;
}

SciQLopColorMapFunction* SciQLopPlot::add_color_map(GetDataPyCallable&& callable,
                                                    const QString& name, bool y_log_scale,
                                                    bool z_log_scale)
{
    if (m_color_map == nullptr)
    {
        m_color_map = new SciQLopColorMapFunction(
            this, this->m_axes[0], this->m_axes[3],
            static_cast<SciQLopPlotColorScaleAxis*>(this->m_axes[4]), std::move(callable),
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
    QCustomPlot::replot(priority);
}

void SciQLopPlot::mousePressEvent(QMouseEvent* event)
{
    QCustomPlot::mousePressEvent(event);
}

void SciQLopPlot::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() != Qt::NoButton)
    {
        QCustomPlot::mouseMoveEvent(event);
        return;
    }
    QCustomPlot::mouseMoveEvent(event);
    // Throttle hover updates — plottableAt + itemAt + selectTest + overlay
    // replot are expensive at 1000+ Hz.  60 Hz is plenty for a tooltip.
    if (m_hover_throttle_timer.isValid() && m_hover_throttle_timer.elapsed() < 16)
        return;
    m_hover_throttle_timer.start();
    _update_mouse_cursor(event);
    m_crosshair->update_at_pixel(event->pos());
    if (!std::isnan(m_crosshair->current_key()))
        emit hover_x_changed(m_crosshair->current_key());
}

void SciQLopPlot::enterEvent(QEnterEvent* event)
{
    event->accept();
    QCustomPlot::enterEvent(event);
}

void SciQLopPlot::leaveEvent(QEvent* event)
{
    m_crosshair->hide();
    emit hover_x_changed(std::numeric_limits<double>::quiet_NaN());
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
        this->replot(rpQueuedReplot);
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
    const auto wheelSteps = _signed_length(event->angleDelta()) / 120.0;
    auto* ar = this->axisRect();

    // Check axes directly — avoids layerableListAt() which triggers expensive
    // selectTest() on every plottable (O(N) per data point in QCPGraph2).
    for (auto axisType : { QCPAxis::atBottom, QCPAxis::atLeft, QCPAxis::atTop, QCPAxis::atRight })
    {
        auto* ax = ar->axis(axisType);
        if (ax && ax->selectTest(pos, false) >= 0)
        {
            this->_wheel_zoom(ax, wheelSteps, pos);
            event->accept();
            this->replot(rpQueuedReplot);
            return;
        }
    }

    if (ar->rect().contains(pos.toPoint()))
    {
        if (event->modifiers().testFlags(Qt::ShiftModifier | Qt::ControlModifier))
            this->_wheel_pan(ar->axis(QCPAxis::atLeft), wheelSteps);
        else if (event->modifiers().testFlag(Qt::ControlModifier))
            this->_wheel_zoom(ar->axis(QCPAxis::atBottom), wheelSteps, pos);
        else if (event->modifiers().testFlag(Qt::ShiftModifier))
            this->_wheel_zoom(ar->axis(QCPAxis::atLeft), wheelSteps, pos);
        else
            this->_wheel_pan(ar->axis(QCPAxis::atBottom), wheelSteps);
        this->replot(rpQueuedReplot);
        event->accept();
        return;
    }

    // Color scale: zoom directly instead of going through layerableListAt()
    // which calls selectTest() on every plottable.
    if (m_color_scale && m_color_scale->visible()
        && m_color_scale->selectTest(pos, false) >= 0)
    {
        auto* colorAxis = m_color_scale->axis();
        if (colorAxis)
        {
            this->_wheel_zoom(colorAxis, wheelSteps, pos);
            this->replot(rpQueuedReplot);
        }
        event->accept();
        return;
    }

    QCustomPlot::wheelEvent(event);
}

void SciQLopPlot::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        auto items = selectedItems();
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
    // QRhiWidget::initialize() already runs before resizeEvent on every resize,
    // handling pipeline invalidation, viewport update, and an immediate replot.
    // Calling QCustomPlot::resizeEvent would do a redundant second immediate
    // replot — very expensive during continuous splitter drag.
    setViewport(rect());
    replot(rpQueuedReplot);
    Q_EMIT resized(event->size());
}

void SciQLopPlot::set_crosshair_enabled(bool enabled)
{
    m_crosshair->set_enabled(enabled);
}

bool SciQLopPlot::crosshair_enabled() const
{
    return m_crosshair->enabled();
}

void SciQLopPlot::show_crosshair_at_key(double key)
{
    if (!m_crosshair->enabled())
        return;
    m_crosshair->update_at_key(key);
}

void SciQLopPlot::hide_crosshair()
{
    m_crosshair->hide();
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
    QToolTip::hideText();
    return true;
}

void SciQLopPlot::_register_plottable_wrapper(SciQLopPlottableInterface* plottable)
{
    m_plottables.append(plottable);
    connect(plottable, &SciQLopGraphInterface::destroyed, this,
            [this, plottable]()
            {
                m_plottables.removeOne(plottable);
                if (m_color_map == plottable)
                    m_color_map = nullptr;
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
        applyTheme();
    }
}

void _impl::SciQLopPlot::_ensure_colorscale_is_visible(SciQLopHistogram2D* hist)
{
    if (!m_color_scale->visible())
    {
        m_color_scale->setVisible(true);
        plotLayout()->addElement(0, 1, m_color_scale);
        hist->histogram()->setColorScale(m_color_scale);
        applyTheme();
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
    else if (auto* gi = dynamic_cast<QCPGroupLegendItem*>(item); gi != nullptr)
    {
        auto* mg = gi->multiGraph();
        if (!mg)
            return;
        int comp = gi->selectedComponent();
        if (comp >= 0 && comp < mg->componentCount())
        {
            mg->component(comp).visible = !mg->component(comp).visible;
        }
        else
        {
            bool anyVisible = false;
            for (int i = 0; i < mg->componentCount(); ++i)
                anyVisible |= mg->component(i).visible;
            for (int i = 0; i < mg->componentCount(); ++i)
                mg->component(i).visible = !anyVisible;
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

SciQLopHistogram2D* SciQLopPlot::add_histogram2d(const QString& name, int key_bins, int value_bins)
{
    auto hist = m_impl->add_histogram2d(name, key_bins, value_bins);
    if (hist)
    {
        _configure_color_map(hist, false, false);
    }
    return hist;
}

SciQLopHistogram2DFunction* SciQLopPlot::add_histogram2d(GetDataPyCallable&& callable,
                                                          const QString& name, int key_bins,
                                                          int value_bins)
{
    auto hist = m_impl->add_histogram2d(std::move(callable), name, key_bins, value_bins);
    if (hist)
    {
        _configure_color_map(hist, false, false);
        _connect_callable_sync(hist, nullptr);
    }
    return hist;
}

SciQLopWaterfallGraph* SciQLopPlot::add_waterfall(const QString& name, const QStringList& labels,
                                                   const QList<QColor>& colors)
{
    auto* wf = m_impl->add_plottable<SciQLopWaterfallGraph>(labels);
    if (wf)
    {
        wf->set_name(name);
        if (!colors.isEmpty())
            wf->set_colors(colors);
    }
    return wf;
}

SciQLopWaterfallGraphFunction* SciQLopPlot::add_waterfall(GetDataPyCallable callable,
                                                          const QString& name,
                                                          const QStringList& labels,
                                                          const QList<QColor>& colors)
{
    auto* wf = m_impl->add_plottable<SciQLopWaterfallGraphFunction>(std::move(callable), labels);
    if (wf)
    {
        wf->set_name(name);
        if (!colors.isEmpty())
            wf->set_colors(colors);
        _connect_callable_sync(wf, nullptr);
    }
    return wf;
}

SciQLopOverlay* SciQLopPlot::overlay()
{
    if (!m_overlay)
        m_overlay = new SciQLopOverlay(m_impl->overlay(), this);
    return m_overlay;
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

void SciQLopPlot::enable_cursor(bool enable) noexcept
{
    m_impl->set_crosshair_enabled(enable);
}

void SciQLopPlot::set_theme(SciQLopTheme* theme)
{
    if (m_theme)
        disconnect(m_theme, nullptr, this, nullptr);

    m_theme = theme;

    if (theme && theme->qcp_theme())
    {
        m_impl->setTheme(theme->qcp_theme());
        m_impl->crosshair()->apply_theme(theme->qcp_theme());
        connect(theme->qcp_theme(), &QCPTheme::changed, this,
                [this]() {
                    if (m_theme && m_theme->qcp_theme())
                        m_impl->crosshair()->apply_theme(m_theme->qcp_theme());
                });
        connect(theme, &QObject::destroyed, this, [this]() { m_impl->setTheme(nullptr); });
    }
    else
    {
        m_impl->setTheme(nullptr);
    }
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
                                              QStringList labels, QList<QColor> colors,
                                              GraphType graph_type, GraphMarkerShape marker,
                                              QVariantMap metaData)
{
    SQPQCPAbstractPlottableWrapper* plottable = nullptr;
    switch (graph_type)
    {
        case GraphType::Line:
        case GraphType::Scatter:
            if (y.ndim() <= 1 || y.size(1) == 1)
                plottable = m_impl->add_plottable<SciQLopSingleLineGraph>(labels, metaData);
            else
                plottable = m_impl->add_plottable<SciQLopLineGraph>(labels, metaData);
            break;
        case GraphType::ParametricCurve:
            plottable = m_impl->add_plottable<SciQLopCurve>(labels, metaData);
            break;
        case GraphType::Waterfall:
            plottable = m_impl->add_plottable<SciQLopWaterfallGraph>(labels, metaData);
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
    if (!cm)
        return nullptr;
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
            if (auto z_axis = static_cast<SciQLopPlotColorScaleAxis*>(cmap->z_axis()))
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
            if (labels.size() <= 1)
                plottable = m_impl->add_plottable<SciQLopSingleLineGraphFunction>(
                    std::move(callable), labels, metaData);
            else
                plottable = m_impl->add_plottable<SciQLopLineGraphFunction>(
                    std::move(callable), labels, metaData);
            break;
        case GraphType::ParametricCurve:
            plottable = m_impl->add_plottable<SciQLopCurveFunction>(std::move(callable), labels,
                                                                    metaData);
            break;
        case GraphType::Waterfall:
            plottable = m_impl->add_plottable<SciQLopWaterfallGraphFunction>(
                std::move(callable), labels, metaData);
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

SciQLopColorMapInterface* SciQLopPlot::plot_impl(const PyBuffer& x, const PyBuffer& y,
                                                  QString name, int key_bins, int value_bins,
                                                  QVariantMap metaData)
{
    auto* hist = m_impl->add_histogram2d(name, key_bins, value_bins);
    if (hist)
    {
        hist->set_meta_data(metaData);
        _configure_color_map(hist, false, false);
        hist->set_data(x, y);
    }
    return hist;
}

SciQLopColorMapInterface* SciQLopPlot::plot_impl(GetDataPyCallable callable, QString name,
                                                  int key_bins, int value_bins,
                                                  QObject* sync_with, QVariantMap metaData)
{
    auto* hist = m_impl->add_histogram2d(std::move(callable), name, key_bins, value_bins);
    if (hist)
    {
        hist->set_meta_data(metaData);
        _configure_color_map(hist, false, false);
        _connect_callable_sync(hist, sync_with);
    }
    return hist;
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

bool SciQLopPlot::save_pdf(const QString& filename, int width, int height)
{
    return m_impl->savePdf(filename, width, height);
}

bool SciQLopPlot::save_png(const QString& filename, int width, int height,
                           double scale, int quality)
{
    return m_impl->savePng(filename, width, height, scale, quality);
}

bool SciQLopPlot::save_jpg(const QString& filename, int width, int height,
                           double scale, int quality)
{
    return m_impl->saveJpg(filename, width, height, scale, quality);
}

bool SciQLopPlot::save_bmp(const QString& filename, int width, int height,
                           double scale)
{
    return m_impl->saveBmp(filename, width, height, scale);
}

bool SciQLopPlot::save(const QString& filename, int width, int height,
                       double scale, int quality)
{
    auto ext = QFileInfo(filename).suffix().toLower();
    if (ext == "pdf")
        return save_pdf(filename, width, height);
    if (ext == "png")
        return save_png(filename, width, height, scale, quality);
    if (ext == "jpg" || ext == "jpeg")
        return save_jpg(filename, width, height, scale, quality);
    if (ext == "bmp")
        return save_bmp(filename, width, height, scale);
    return false;
}

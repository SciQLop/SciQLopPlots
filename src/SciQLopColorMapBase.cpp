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
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include <QMouseEvent>
#include <layoutelements/layoutelement-legend.h>
#include <algorithm>
#include <cmath>

SciQLopColorMapBase::SciQLopColorMapBase(SciQLopPlotAxis* keyAxis, SciQLopPlotAxis* valueAxis,
                                         SciQLopPlotColorScaleAxis* colorScaleAxis,
                                         QVariantMap metadata, QObject* parent)
    : SciQLopColorMapInterface(std::move(metadata), parent)
    , _keyAxis{keyAxis}
    , _valueAxis{valueAxis}
    , _colorScaleAxis{colorScaleAxis}
{
    _connect_legend_visibility();
    _forward_axis_signals();
    if (_colorScaleAxis)
    {
        connect(_colorScaleAxis, &SciQLopPlotAxis::log_changed, this,
                &SciQLopColorMapInterface::z_log_scale_changed);
        connect(_colorScaleAxis, &SciQLopPlotColorScaleAxis::color_gradient_changed, this,
                &SciQLopColorMapInterface::gradient_changed);
    }
}

void SciQLopColorMapBase::_forward_axis_signals()
{
    disconnect(_y_log_forward);
    if (_valueAxis)
        _y_log_forward = connect(_valueAxis, &SciQLopPlotAxis::log_changed, this,
                                 &SciQLopColorMapInterface::y_log_scale_changed);
}

SciQLopColorMapBase::~SciQLopColorMapBase()
{
    // Our QCP children die after this class: their teardown changes the shared colour scale,
    // whose signals must not reach this half-destroyed map.
    disconnect(_y_log_forward);
    if (_colorScaleAxis)
        disconnect(_colorScaleAxis, nullptr, this, nullptr);
    // Unhook the rescale provider so the color-scale axis won't call back into
    // our (half-destroyed) z_rescale_range -- but only if we're still the
    // plottable that owns the shared slot. A colormap and a histogram2d (or
    // two histograms) can coexist on one plot, sharing this one axis; without
    // the ownership check, destroying the NON-owner would still clobber
    // whichever plottable is the current owner. Safe if the axis has already
    // been destroyed thanks to QPointer.
    if (_colorScaleAxis)
        _colorScaleAxis->clear_rescale_range_provider(this);
}

void SciQLopColorMapBase::_connect_legend_visibility()
{
    if (auto* plot = _plot())
        connect(plot, &QCustomPlot::legendDoubleClick, this,
                &SciQLopColorMapBase::_on_legend_double_clicked);
    connect(this, &SciQLopPlottableInterface::visible_changed, this,
            &SciQLopColorMapBase::_apply_legend_visibility_style);
}

void SciQLopColorMapBase::_on_legend_double_clicked(QCPLegend* legend,
                                                    QCPAbstractLegendItem* item,
                                                    QMouseEvent* event)
{
    Q_UNUSED(legend);
    Q_UNUSED(event);
    if (auto* li = dynamic_cast<QCPPlottableLegendItem*>(item);
        li != nullptr && li == _legend_item())
        set_visible(!visible());
}

void SciQLopColorMapBase::_apply_legend_visibility_style(bool visible)
{
    if (auto* li = _legend_item(); li != nullptr)
    {
        // Restore the legend's themed text colour when visible; dim via alpha
        // when hidden so it reads correctly on both light and dark themes
        // (never hard-code black/gray — black is invisible on a dark legend).
        QColor c = li->parentLegend() ? li->parentLegend()->textColor() : li->textColor();
        if (!visible)
            c.setAlpha(90);
        li->setTextColor(c);
        li->setSelectedTextColor(c);
    }
}

void SciQLopColorMapBase::_set_selected(bool selected)
{
    m_selected = selected;
    if (auto* legend_item = _legend_item(); legend_item && legend_item->selected() != selected)
        legend_item->setSelected(selected);
}

void SciQLopColorMapBase::set_autoscale_percentile_low(double percentile) noexcept
{
    if (std::isnan(percentile))
        return;
    const double clamped = std::clamp(percentile, 0., 100.);
    if (clamped == _autoscale_percentile_low)
        return;
    _autoscale_percentile_low = clamped;
    Q_EMIT autoscale_percentile_low_changed(clamped);
}

void SciQLopColorMapBase::set_autoscale_percentile_high(double percentile) noexcept
{
    if (std::isnan(percentile))
        return;
    const double clamped = std::clamp(percentile, 0., 100.);
    if (clamped == _autoscale_percentile_high)
        return;
    _autoscale_percentile_high = clamped;
    Q_EMIT autoscale_percentile_high_changed(clamped);
}

std::optional<SciQLopPlotRange> SciQLopColorMapBase::z_rescale_range() const noexcept
{
    if (_autoscale_percentile_low <= 0. && _autoscale_percentile_high >= 100.)
        return std::nullopt;
    if (!_keyAxis || !_valueAxis)
        return std::nullopt;
    auto r = z_percentile_range(_keyAxis->range(), _valueAxis->range(),
                                _autoscale_percentile_low, _autoscale_percentile_high);
    if (std::isnan(r.start()) || std::isnan(r.stop()))
        return std::nullopt;
    // A degenerate (zero-width) percentile band -- e.g. a sparse spectrogram
    // where the visible cells are >98% one value -- silently no-ops the
    // colorbar: QCPAxis::setRange rejects a zero-width range (validRange
    // requires size > minRange), but leaves QCPColorScale::dataRange()
    // clobbered with it regardless. Fall through to the min/max fallback
    // instead, mirroring the value-axis path's zero-width guard.
    if (r.start() == r.stop())
        return std::nullopt;
    return r;
}

void SciQLopColorMapBase::install_rescale_provider() noexcept
{
    if (_colorScaleAxis)
        _colorScaleAxis->set_rescale_range_provider(this, [this]() { return z_rescale_range(); });
}

void SciQLopColorMapBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) {
        if (auto* p = plottable(); p) p->setKeyAxis(a);
    });
}

void SciQLopColorMapBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) {
        if (auto* p = plottable(); p) p->setValueAxis(a);
    });
    _forward_axis_signals();
}

void SciQLopColorMapBase::set_auto_scale_y(bool auto_scale_y)
{
    if (auto_scale_y == _auto_scale_y)
        return;
    _auto_scale_y = auto_scale_y;
    Q_EMIT auto_scale_y_changed(auto_scale_y);
}

void SciQLopColorMapBase::_auto_scale_y_to(const QCPRange& range, bool found)
{
    // Route through set_range (not QCPAxis::setRange directly) so clamp_range and
    // m_last_valid_range stay consistent: with a max/min range-size limit configured, a direct
    // setRange makes the rangeChanged handler see clamped != requested and revert to the old
    // range, so auto-scale-y silently does nothing.
    if (_auto_scale_y && _valueAxis && found && range.lower != range.upper)
        _valueAxis->set_range(SciQLopPlotRange(range.lower, range.upper));
}

namespace
{
//! The NeoQCP plottables with contours share the contour API, not a base class.
template <typename F>
bool with_contours(QCPAbstractPlottable* p, F&& f)
{
    if (auto* map = qobject_cast<QCPColorMap2*>(p))
        f(map);
    else if (auto* hist = qobject_cast<QCPHistogram2D*>(p))
        f(hist);
    else
        return false;
    return true;
}

template <typename T, typename F>
T read_contours(QCPAbstractPlottable* p, T fallback, F&& f)
{
    with_contours(p, [&](auto* c) { fallback = f(c); });
    return fallback;
}
}

void SciQLopColorMapBase::set_contour_levels(const QVector<double>& levels)
{
    SCIQLOP_ON_OWNER_THREAD(set_contour_levels(levels));
    if (with_contours(plottable(), [&](auto* c) { c->setContourLevels(levels); }))
        Q_EMIT contour_levels_changed();
}

QVector<double> SciQLopColorMapBase::contour_levels() const
{
    return read_contours(plottable(), QVector<double> {}, [](auto* c) { return c->contourLevels(); });
}

void SciQLopColorMapBase::set_auto_contour_levels(int count)
{
    SCIQLOP_ON_OWNER_THREAD(set_auto_contour_levels(count));
    if (with_contours(plottable(), [&](auto* c) { c->setAutoContourLevels(count); }))
        Q_EMIT contour_levels_changed();
}

int SciQLopColorMapBase::auto_contour_level_count() const
{
    return read_contours(plottable(), 0, [](auto* c) { return c->autoContourLevelCount(); });
}

void SciQLopColorMapBase::set_contour_pen(const QPen& pen)
{
    SCIQLOP_ON_OWNER_THREAD(set_contour_pen(pen));
    if (with_contours(plottable(), [&](auto* c) { c->setContourPen(pen); }))
        Q_EMIT contour_pen_changed(pen);
}

QPen SciQLopColorMapBase::contour_pen() const
{
    return read_contours(plottable(), QPen(), [](auto* c) { return c->contourPen(); });
}

void SciQLopColorMapBase::set_contour_color(const QColor& color)
{
    SCIQLOP_ON_OWNER_THREAD(set_contour_color(color));
    auto pen = contour_pen();
    pen.setColor(color);
    set_contour_pen(pen);
}

QColor SciQLopColorMapBase::contour_color() const
{
    return read_contours(plottable(), QColor(), [](auto* c) { return c->contourPen().color(); });
}

void SciQLopColorMapBase::set_contour_width(double width)
{
    SCIQLOP_ON_OWNER_THREAD(set_contour_width(width));
    auto pen = contour_pen();
    pen.setWidthF(width);
    set_contour_pen(pen);
}

double SciQLopColorMapBase::contour_width() const
{
    return read_contours(plottable(), 1.0, [](auto* c) { return c->contourPen().widthF(); });
}

void SciQLopColorMapBase::set_contour_labels_enabled(bool enabled)
{
    SCIQLOP_ON_OWNER_THREAD(set_contour_labels_enabled(enabled));
    if (with_contours(plottable(), [&](auto* c) { c->setContourLabelEnabled(enabled); }))
        Q_EMIT contour_labels_enabled_changed(enabled);
}

bool SciQLopColorMapBase::contour_labels_enabled() const
{
    return read_contours(plottable(), false, [](auto* c) { return c->contourLabelEnabled(); });
}

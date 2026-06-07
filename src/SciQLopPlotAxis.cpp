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
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

#include "SciQLopPlots/PercentileMath.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/Tracing.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include "qcustomplot.h"
#include <algorithm>
#include <plottables/plottable-colormap2.h>
#include <plottables/plottable-histogram2d.h>
#include <cmath>
#include <vector>

SciQLopPlotRange SciQLopPlotAxisInterface::clamp_range(const SciQLopPlotRange& range) const noexcept
{
    auto sz = range.size();
    auto c = range.center();
    if (sz > m_max_range_size)
        return SciQLopPlotRange(c - m_max_range_size / 2.0, c + m_max_range_size / 2.0,
                                _is_time_axis);
    if (m_min_range_size > 0.0 && sz < m_min_range_size)
        return SciQLopPlotRange(c - m_min_range_size / 2.0, c + m_min_range_size / 2.0,
                                _is_time_axis);
    return range;
}

void SciQLopPlotAxisInterface::set_max_range_size(double max_size) noexcept
{
    m_max_range_size = max_size > 0.0 ? max_size : std::numeric_limits<double>::infinity();
}

void SciQLopPlotAxisInterface::set_min_range_size(double min_size) noexcept
{
    m_min_range_size = min_size > 0.0 ? min_size : 0.0;
}

SciQLopPlotAxis::SciQLopPlotAxis(QCPAxis* axis, QObject* parent, bool is_time_axis,
                                 const QString& name)
        : SciQLopPlotAxisInterface(parent, name), m_axis(axis)
{
    _is_time_axis = is_time_axis;
    connect(axis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this,
            [this](const QCPRange& range)
            {
                if (m_suppress_range_signals)
                    return;
                SciQLopPlotRange requested { range.lower, range.upper, _is_time_axis };
                auto clamped = clamp_range(requested);
                if (clamped != requested)
                {
                    if (m_last_valid_range.is_valid())
                        m_axis->setRange(m_last_valid_range.start(), m_last_valid_range.stop());
                    Q_EMIT range_clamped(requested, m_last_valid_range);
                    return;
                }
                m_last_valid_range = clamped;
                Q_EMIT range_changed(clamped);
            });

    connect(axis, &QCPAxis::selectionChanged, this,
            [this](const QCPAxis::SelectableParts& parts)
            {
                Q_EMIT selection_changed(parts.testAnyFlags(QCPAxis::spAxis | QCPAxis::spTickLabels
                                                            | QCPAxis::spAxisLabel));
            });
}

void SciQLopPlotAxis::set_range(const SciQLopPlotRange& range) noexcept
{
    if (!m_axis.isNull() && range.is_valid())
    {
        auto clamped = clamp_range(range);
        if (m_axis->range().lower != clamped.start() || m_axis->range().upper != clamped.stop())
        {
            m_last_valid_range = clamped;
            m_axis->setRange(clamped.start(), clamped.stop());
            m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
        if (clamped != range)
            Q_EMIT range_clamped(range, clamped);
    }
}

void SciQLopPlotAxis::set_visible(bool visible) noexcept
{
    if (!m_axis.isNull() && m_axis->visible() != visible)
    {
        m_axis->setVisible(visible);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT visible_changed(visible);
    }
}

void SciQLopPlotAxis::set_log(bool log) noexcept
{
    if (!m_axis.isNull() && (m_axis->scaleType() == QCPAxis::stLogarithmic) != log)
    {
        {
            // Block signals during scale type + ticker change to prevent
            // rangeChanged from triggering margin recomputation with stale tick vectors
            const QSignalBlocker blocker(m_axis);
            if (log)
            {
                m_axis->setScaleType(QCPAxis::stLogarithmic);
                m_axis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
            }
            else
            {
                m_axis->setScaleType(QCPAxis::stLinear);
                m_axis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
            }
        }
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT log_changed(log);
    }
}

void SciQLopPlotAxis::set_label(const QString& label) noexcept
{
    if (!m_axis.isNull() && m_axis->label() != label)
    {
        m_axis->setLabel(label);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT label_changed(label);
    }
}

void SciQLopPlotAxis::set_selected(bool selected) noexcept
{
    if (!m_axis.isNull() && (this->selected() != selected))
    {
        if (selected)
            m_axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels
                                     | QCPAxis::spAxisLabel);
        else
            m_axis->setSelectedParts(QCPAxis::spNone);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT selection_changed(selected);
    }
}

void SciQLopPlotAxis::set_tick_labels_visible(bool visible) noexcept
{
    if (!m_axis.isNull() && m_axis->tickLabels() != visible)
    {
        m_axis->setTickLabels(visible);
        if (!visible)
        {
            m_axis->setTickLabelPadding(0);
            m_axis->setPadding(std::max(1, m_axis->basePen().width()));
        }
        else
        {
            m_axis->setTickLabelPadding(5);
            m_axis->setPadding(5);
        }
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT tick_labels_visible_changed(visible);
    }
}

void SciQLopPlotAxis::set_label_font(const QFont& font) noexcept
{
    if (!m_axis.isNull() && m_axis->labelFont() != font)
    {
        m_axis->setLabelFont(font);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT label_font_changed(font);
    }
}

void SciQLopPlotAxis::set_label_color(const QColor& color) noexcept
{
    if (!m_axis.isNull() && m_axis->labelColor() != color)
    {
        m_axis->setLabelColor(color);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT label_color_changed(color);
    }
}

void SciQLopPlotAxis::set_tick_label_font(const QFont& font) noexcept
{
    if (!m_axis.isNull() && m_axis->tickLabelFont() != font)
    {
        m_axis->setTickLabelFont(font);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT tick_label_font_changed(font);
    }
}

void SciQLopPlotAxis::set_tick_label_color(const QColor& color) noexcept
{
    if (!m_axis.isNull() && m_axis->tickLabelColor() != color)
    {
        m_axis->setTickLabelColor(color);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT tick_label_color_changed(color);
    }
}

QFont SciQLopPlotAxis::label_font() const noexcept
{
    return m_axis.isNull() ? QFont() : m_axis->labelFont();
}

QColor SciQLopPlotAxis::label_color() const noexcept
{
    return m_axis.isNull() ? QColor() : m_axis->labelColor();
}

QFont SciQLopPlotAxis::tick_label_font() const noexcept
{
    return m_axis.isNull() ? QFont() : m_axis->tickLabelFont();
}

QColor SciQLopPlotAxis::tick_label_color() const noexcept
{
    return m_axis.isNull() ? QColor() : m_axis->tickLabelColor();
}

SciQLopPlotRange SciQLopPlotAxis::range() const noexcept
{
    if (m_axis.isNull())
        return SciQLopPlotRange();
    return SciQLopPlotRange(m_axis->range().lower, m_axis->range().upper, _is_time_axis);
}

bool SciQLopPlotAxis::visible() const noexcept
{
    if (m_axis.isNull())
        return false;
    return m_axis->visible();
}

bool SciQLopPlotAxis::log() const noexcept
{
    if (m_axis.isNull())
        return false;
    return m_axis->scaleType() == QCPAxis::stLogarithmic;
}

QString SciQLopPlotAxis::label() const noexcept
{
    if (m_axis.isNull())
        return QString();
    return m_axis->label();
}

bool SciQLopPlotAxis::tick_labels_visible() const noexcept
{
    if (m_axis.isNull())
        return false;
    return m_axis->tickLabels();
}

Qt::Orientation SciQLopPlotAxis::orientation() const noexcept
{
    if (m_axis.isNull())
        return Qt::Horizontal;
    return m_axis->orientation();
}

Qt::Axis SciQLopPlotAxis::axis() const noexcept
{
    if (m_axis.isNull())
        return Qt::XAxis;
    if (orientation() == Qt::Horizontal)
        return Qt::XAxis;
    return Qt::YAxis;
}

Qt::AnchorPoint SciQLopPlotAxis::anchor() const noexcept
{
    if (m_axis.isNull())
        return Qt::AnchorBottom;
    switch (m_axis->axisType())
    {
        case QCPAxis::AxisType::atLeft:
            return Qt::AnchorLeft;
            break;
        case QCPAxis::AxisType::atRight:
            return Qt::AnchorRight;
            break;
        case QCPAxis::AxisType::atTop:
            return Qt::AnchorTop;
            break;
        case QCPAxis::AxisType::atBottom:
            return Qt::AnchorBottom;
            break;
        default:
            break;
    }
    return Qt::AnchorBottom;
}

bool SciQLopPlotAxis::selected() const noexcept
{
    if (m_axis.isNull())
        return false;
    return m_axis->selectedParts().testFlag(QCPAxis::spAxis);
}

void SciQLopPlotAxis::set_autoscale_percentile_low(double percentile) noexcept
{
    if (std::isnan(percentile))
        return;
    m_autoscale_percentile_low = std::clamp(percentile, 0., 100.);
}

void SciQLopPlotAxis::set_autoscale_percentile_high(double percentile) noexcept
{
    if (std::isnan(percentile))
        return;
    m_autoscale_percentile_high = std::clamp(percentile, 0., 100.);
}

void SciQLopPlotAxis::rescale() noexcept
{
    if (m_axis.isNull())
        return;

    // For value axes, restrict rescale to the visible key range
    // so Y auto-scale only fits data currently in view
    QCPRange newRange;
    bool haveRange = false;
    QCP::SignDomain signDomain = QCP::sdBoth;
    const bool is_log = m_axis->scaleType() == QCPAxis::stLogarithmic;
    if (is_log)
        signDomain = (m_axis->range().upper < 0 ? QCP::sdNegative : QCP::sdPositive);

    // Robust autoscale path: pool finite y-values across graphs on this axis,
    // then clip to the configured percentile band. Uses the plot's own
    // authoritative plottable list (sqp_plottables) so it can't miss anything
    // the plot itself tracks. Only kicks in when percentiles deviate from
    // 0/100; otherwise we fall through to the plain min/max loop below.
    const bool wants_percentile
        = (m_autoscale_percentile_low > 0. || m_autoscale_percentile_high < 100.);
    if (auto* plot = qobject_cast<_impl::SciQLopPlot*>(m_axis->parentPlot()); wants_percentile && plot)
    {
        ::SciQLopPlots::tracing::ScopedZone _sz("axis.rescale.percentile", "rescale");
        std::vector<double> pooled;
        for (auto* p : plot->sqp_plottables())
        {
            // qobject_cast to SciQLopGraphInterface deliberately excludes
            // colormaps/histograms: their y axis carries positional metadata
            // (bin edges, frequency rows), not data values to pool. The
            // legacy min/max fallthrough still considers them via QCP's own
            // getValueRange path.
            auto* graph = qobject_cast<SciQLopGraphInterface*>(p);
            if (!graph || graph->y_axis() != this)
                continue;
            auto* keyAxis = qobject_cast<SciQLopPlotAxis*>(graph->x_axis());
            if (!keyAxis || !keyAxis->qcp_axis())
                continue;
            const auto kr = keyAxis->qcp_axis()->range();
            graph->collect_visible_values(SciQLopPlotRange(kr.lower, kr.upper), pooled);
        }
        if (is_log)
        {
            const auto cutoff
                = [&](double v) { return signDomain == QCP::sdNegative ? v >= 0. : v <= 0.; };
            pooled.erase(std::remove_if(pooled.begin(), pooled.end(), cutoff), pooled.end());
        }
        _sz.add_arg("pool_size", static_cast<int64_t>(pooled.size()));
        if (!pooled.empty())
        {
            const auto r = sciqlop::percentile::percentile_range(
                pooled, m_autoscale_percentile_low, m_autoscale_percentile_high);
            // Degenerate (zero-width) percentile ranges happen when data has
            // less precision than double inside the band (e.g. uint64 values
            // larger than 2^53). QCPAxis::setRange silently rejects them, so
            // we fall through to min/max rather than appear no-op.
            if (!std::isnan(r.start()) && !std::isnan(r.stop()) && r.start() != r.stop())
            {
                // Route through set_range so clamp_range, m_last_valid_range,
                // and the queued replot stay consistent with manual range
                // changes (avoids the rangeChanged lambda reverting the new
                // range when it exceeds m_max_range_size).
                set_range(r);
                return;
            }
        }
    }

    for (auto* plottable : m_axis->plottables())
    {
        if (!plottable->realVisibility())
            continue;
        bool found = false;
        QCPRange plottableRange;
        if (plottable->keyAxis() == m_axis)
        {
            plottableRange = plottable->getKeyRange(found, signDomain);
        }
        else
        {
            // Value axis: restrict to visible key range
            auto keyRange = plottable->keyAxis()->range();
            plottableRange = plottable->getValueRange(found, signDomain, keyRange);
        }
        if (found)
        {
            if (!haveRange)
                newRange = plottableRange;
            else
                newRange.expand(plottableRange);
            haveRange = true;
        }
    }
    if (haveRange)
    {
        if (!QCPRange::validRange(newRange))
        {
            double center = (newRange.lower + newRange.upper) * 0.5;
            newRange.lower = center - m_axis->range().size() / 2.0;
            newRange.upper = center + m_axis->range().size() / 2.0;
        }
        // Route through set_range (not m_axis->setRange directly) so clamp_range
        // and m_last_valid_range stay consistent: with a max/min range-size limit
        // configured, a direct setRange makes the rangeChanged handler see
        // clamped != requested and revert to the old range, so 'm' silently does
        // nothing. Mirrors the percentile path above.
        set_range(SciQLopPlotRange(newRange.lower, newRange.upper, _is_time_axis));
        return;
    }
    m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

QCPAxis* SciQLopPlotAxis::qcp_axis() const noexcept
{
    return m_axis.data();
}

void SciQLopPlotDummyAxis::set_range(const SciQLopPlotRange& range) noexcept
{
    auto clamped = clamp_range(range);
    if (m_range != clamped)
    {
        m_range = clamped;
        Q_EMIT this->range_changed(clamped);
    }
    if (clamped != range)
        Q_EMIT range_clamped(range, clamped);
}

SciQLopPlotColorScaleAxis::SciQLopPlotColorScaleAxis(QCPColorScale* axis, QObject* parent,
                                                     const QString& name)
        : SciQLopPlotAxis(axis->axis(), parent, false, name), m_axis(axis)
{
    connect(axis, QOverload<const QCPRange&>::of(&QCPColorScale::dataRangeChanged), this,
            [this](const QCPRange& range)
            { Q_EMIT range_changed(SciQLopPlotRange { range.lower, range.upper }); });
}

void SciQLopPlotColorScaleAxis::set_range(const SciQLopPlotRange& range) noexcept
{
    if (!m_axis.isNull()
        && (m_axis->dataRange().lower != range.start()
            || m_axis->dataRange().upper != range.stop()))
    {
        m_axis->setDataRange(QCPRange(range.start(), range.stop()));
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SciQLopPlotColorScaleAxis::set_visible(bool visible) noexcept
{
    if (!m_axis.isNull() && m_axis->visible() != visible)
    {
        m_axis->setVisible(visible);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT visible_changed(visible);
    }
}

void SciQLopPlotColorScaleAxis::set_log(bool log) noexcept
{
    if (!m_axis.isNull() && (m_axis->dataScaleType() == QCPAxis::stLogarithmic) != log)
    {
        {
            // Block signals during scale type + ticker change to prevent
            // rangeChanged from triggering margin recomputation with stale tick vectors
            const QSignalBlocker blocker(m_axis.data()->axis());
            if (log)
            {
                m_axis->setDataScaleType(QCPAxis::stLogarithmic);
                m_axis.data()->axis()->setTicker(
                    QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
            }
            else
            {
                m_axis->setDataScaleType(QCPAxis::stLinear);
                m_axis.data()->axis()->setTicker(
                    QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
            }
        }
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT log_changed(log);
    }
}

void SciQLopPlotColorScaleAxis::set_label(const QString& label) noexcept
{
    if (!m_axis.isNull() && m_axis->label() != label)
    {
        m_axis->setLabel(label);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT label_changed(label);
    }
}

void SciQLopPlotColorScaleAxis::set_color_gradient(const ColorGradient gradient) noexcept
{
    if (!m_axis.isNull() && m_color_gradient != gradient)
    {
        m_color_gradient = gradient;
        QCPColorGradient new_gradient = to_qcp(gradient);
        new_gradient.setNanHandling(QCPColorGradient::nhTransparent);
        m_axis->setGradient(new_gradient);
        m_axis->rescaleDataRange(true);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT color_gradient_changed(gradient);
    }
}

SciQLopPlotRange SciQLopPlotColorScaleAxis::range() const noexcept
{
    if (m_axis.isNull())
        return SciQLopPlotRange();
    return SciQLopPlotRange(m_axis->dataRange().lower, m_axis->dataRange().upper);
}

bool SciQLopPlotColorScaleAxis::visible() const noexcept
{
    if (m_axis.isNull())
        return false;
    return m_axis->visible();
}

bool SciQLopPlotColorScaleAxis::log() const noexcept
{
    if (m_axis.isNull())
        return false;
    return m_axis->dataScaleType() == QCPAxis::stLogarithmic;
}

QString SciQLopPlotColorScaleAxis::label() const noexcept
{
    if (m_axis.isNull())
        return QString();
    return m_axis->label();
}

ColorGradient SciQLopPlotColorScaleAxis::color_gradient() const noexcept
{
    return m_color_gradient;
}

Qt::Orientation SciQLopPlotColorScaleAxis::orientation() const noexcept
{
    return Qt::Vertical;
}

Qt::Axis SciQLopPlotColorScaleAxis::axis() const noexcept
{
    return Qt::ZAxis;
}

Qt::AnchorPoint SciQLopPlotColorScaleAxis::anchor() const noexcept
{
    return Qt::AnchorLeft;
}

bool SciQLopPlotColorScaleAxis::selected() const noexcept
{
    if (m_axis.isNull())
        return false;
    return m_axis->axis()->selectedParts().testFlag(QCPAxis::spAxis);
}

void SciQLopPlotColorScaleAxis::rescale() noexcept
{
    if (!m_axis.isNull())
    {
        if (m_rescale_range_provider)
        {
            if (auto r = m_rescale_range_provider(); r.has_value())
            {
                set_range(*r);
                m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
                return;
            }
        }
        // QCPColorScale::rescaleDataRange only finds QCPColorMap, not QCPColorMap2/QCPHistogram2D.
        auto* plot = m_axis->parentPlot();
        for (int i = 0; i < plot->plottableCount(); ++i)
        {
            if (auto* cm2 = qobject_cast<QCPColorMap2*>(plot->plottable(i));
                cm2 && cm2->colorScale() == m_axis.data())
            {
                cm2->rescaleDataRange(true);
            }
            else if (auto* hist = qobject_cast<QCPHistogram2D*>(plot->plottable(i));
                     hist && hist->colorScale() == m_axis.data())
            {
                hist->rescaleDataRange(true);
            }
        }
        // Also try the legacy path for any QCPColorMap instances
        m_axis->rescaleDataRange(true);
        plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

QCPAxis* SciQLopPlotColorScaleAxis::qcp_axis() const noexcept
{
    if (m_axis.isNull())
        return nullptr;
    return m_axis->axis();
}

void SciQLopPlotAxisInterface::couple_range_with(SciQLopPlotAxisInterface* other) noexcept
{
    QObject::connect(this, &SciQLopPlotAxisInterface::range_changed, other,
                     QOverload<const SciQLopPlotRange&>::of(&SciQLopPlotAxisInterface::set_range));
    QObject::connect(other, &SciQLopPlotAxisInterface::range_changed, this,
                     QOverload<const SciQLopPlotRange&>::of(&SciQLopPlotAxisInterface::set_range));
}

void SciQLopPlotAxisInterface::decouple_range_from(SciQLopPlotAxisInterface* other) noexcept
{
    QObject::disconnect(
        this, &SciQLopPlotAxisInterface::range_changed, other,
        QOverload<const SciQLopPlotRange&>::of(&SciQLopPlotAxisInterface::set_range));
    QObject::disconnect(
        other, &SciQLopPlotAxisInterface::range_changed, this,
        QOverload<const SciQLopPlotRange&>::of(&SciQLopPlotAxisInterface::set_range));
}

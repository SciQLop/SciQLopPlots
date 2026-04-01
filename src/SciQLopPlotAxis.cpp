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

#include "SciQLopPlots/qcp_enums.hpp"
#include "qcustomplot.h"
#include <plottables/plottable-colormap2.h>
#include <cmath>

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
        if (clamped != range)
        {
            Q_EMIT range_clamped(range, m_last_valid_range);
            return;
        }
        if (m_axis->range().lower != clamped.start() || m_axis->range().upper != clamped.stop())
        {
            m_last_valid_range = clamped;
            m_axis->setRange(clamped.start(), clamped.stop());
            m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
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

void SciQLopPlotAxis::rescale() noexcept
{
    if (m_axis.isNull())
        return;

    // For value axes, restrict rescale to the visible key range
    // so Y auto-scale only fits data currently in view
    QCPRange newRange;
    bool haveRange = false;
    QCP::SignDomain signDomain = QCP::sdBoth;
    if (m_axis->scaleType() == QCPAxis::stLogarithmic)
        signDomain = (m_axis->range().upper < 0 ? QCP::sdNegative : QCP::sdPositive);

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
        m_axis->setRange(newRange);
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
        // QCPColorScale::rescaleDataRange only finds QCPColorMap, not QCPColorMap2.
        // Rescale QCPColorMap2 instances directly.
        auto* plot = m_axis->parentPlot();
        for (int i = 0; i < plot->plottableCount(); ++i)
        {
            if (auto* cm2 = qobject_cast<QCPColorMap2*>(plot->plottable(i));
                cm2 && cm2->colorScale() == m_axis.data())
            {
                cm2->rescaleDataRange(true);
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

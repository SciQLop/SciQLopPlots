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
#include "SciQLopPlots/Items/SciQLopCrosshair.hpp"
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"
#include <axis/axistickerdatetime.h>
#include <theme.h>
#include <data-locator.h>
#include <plottables/plottable-multigraph.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <chrono>
#include <cmath>

SciQLopCrosshair::SciQLopCrosshair(QCustomPlot* plot)
    : QObject(plot), m_plot(plot)
{
    m_vline = new QCPItemStraightLine(plot);
    m_vline->setLayer("overlay");
    m_vline->setSelectable(false);
    m_vline->setPen(QPen(QColor(128, 128, 128, 180), 1, Qt::DashLine));
    m_vline->setVisible(false);

    m_hline = new QCPItemStraightLine(plot);
    m_hline->setLayer("overlay");
    m_hline->setSelectable(false);
    m_hline->setPen(QPen(QColor(128, 128, 128, 180), 1, Qt::DashLine));
    m_hline->setVisible(false);

    m_tooltip = new QCPItemRichText(plot);
    m_tooltip->setLayer("overlay");
    m_tooltip->setSelectable(false);
    m_tooltip->setClipToAxisRect(false);
    m_tooltip->setPadding(QMargins(6, 4, 6, 4));
    m_tooltip->setBrush(QBrush(QColor(0xee, 0xf2, 0xf7, 220)));
    m_tooltip->setPen(QPen(QColor(0, 0, 0, 150)));
    m_tooltip->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_tooltip->setCornerRadius(4);
    m_tooltip->setVisible(false);
}

SciQLopCrosshair::~SciQLopCrosshair() { }

void SciQLopCrosshair::apply_theme(const QCPTheme* theme)
{
    if (!theme)
        return;
    auto pen = QPen(theme->crosshairColor(), 1, Qt::DashLine);
    m_vline->setPen(pen);
    m_hline->setPen(pen);
    m_tooltip->setBrush(QBrush(theme->tooltipBackground()));
    m_tooltip->setPen(QPen(theme->tooltipBorder()));
    m_tooltip->setColor(theme->tooltipText());
    m_tooltip->setCornerRadius(theme->tooltipCornerRadius());
}

void SciQLopCrosshair::set_enabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
        hide();
}

void SciQLopCrosshair::update_at_pixel(const QPointF& pos)
{
    if (!m_enabled || !m_plot->xAxis)
        return;

    auto* axisRect = m_plot->axisRect();
    if (!axisRect || !axisRect->rect().contains(pos.toPoint()))
    {
        hide();
        return;
    }

    const double key = m_plot->xAxis->pixelToCoord(pos.x());
    const double value = m_plot->yAxis ? m_plot->yAxis->pixelToCoord(pos.y())
                                       : std::numeric_limits<double>::quiet_NaN();
    m_current_value = value;

    update_vline_and_tooltip(key, value, pos);

    if (m_plot->yAxis)
    {
        m_hline->point1->setCoords(0, value);
        m_hline->point2->setCoords(1, value);
        m_hline->setVisible(true);
    }

    replot();
}

void SciQLopCrosshair::update_at_key(double key)
{
    if (!m_enabled || !m_plot->xAxis)
        return;

    update_vline_and_tooltip(key);
    m_hline->setVisible(false);
    replot();
}

void SciQLopCrosshair::update_vline_and_tooltip(double key, double value, const QPointF& pixelPos)
{
    m_current_key = key;
    m_current_value = value;

    m_vline->point1->setCoords(key, 0);
    m_vline->point2->setCoords(key, 1);
    m_vline->setVisible(true);

    const QString html = build_tooltip_html(key, pixelPos);
    if (html.isEmpty())
    {
        m_tooltip->setVisible(false);
    }
    else
    {
        m_tooltip->setHtml(html);
        m_tooltip->setVisible(true);

        auto* axisRect = m_plot->axisRect();
        if (axisRect)
        {
            const double px = m_plot->xAxis->coordToPixel(key);
            const double tooltipW = m_tooltip->contentSize().width();
            const double rightSpace = axisRect->rect().right() - px;
            if (rightSpace < tooltipW + 20)
            {
                m_tooltip->setPositionAlignment(Qt::AlignTop | Qt::AlignRight);
                m_tooltip->position->setPixelPosition(
                    QPointF(px - 12, axisRect->rect().top() + 8));
            }
            else
            {
                m_tooltip->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
                m_tooltip->position->setPixelPosition(
                    QPointF(px + 12, axisRect->rect().top() + 8));
            }
        }
    }
}

void SciQLopCrosshair::hide()
{
    m_current_key = std::numeric_limits<double>::quiet_NaN();
    m_vline->setVisible(false);
    m_hline->setVisible(false);
    m_tooltip->setVisible(false);
    replot();
}

void SciQLopCrosshair::replot()
{
    if (m_vline->layer())
        m_vline->layer()->replot();
}

namespace
{
SciQLopWaterfallGraph* find_waterfall_wrapper(QCustomPlot* plot, QCPWaterfallGraph* raw)
{
    for (auto* w : plot->findChildren<SciQLopWaterfallGraph*>())
    {
        if (w->waterfall_graph() == raw)
            return w;
    }
    return nullptr;
}
}

QString SciQLopCrosshair::build_tooltip_html(double key, const QPointF& pixelPos) const
{
    QCPDataLocator locator;
    const bool havePixel = !std::isnan(pixelPos.x()) && !std::isnan(pixelPos.y());

    QString header;
    if (auto datetime_ticker = m_plot->xAxis->ticker().dynamicCast<QCPAxisTickerDateTime>();
        !datetime_ticker.isNull())
    {
        const auto tp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(static_cast<long long>(key * 1000)));
        header = QString::fromStdString(fmt::format("{:%Y-%m-%d %H:%M:%S}", tp));
    }
    else
    {
        header = QString::fromStdString(fmt::format("x = {:.6g}", key));
    }

    QString lines;
    for (int i = 0; i < m_plot->plottableCount(); ++i)
    {
        auto* plottable = m_plot->plottable(i);
        if (!plottable->visible())
            continue;

        auto* keyAxis = plottable->keyAxis();
        auto* valueAxis = plottable->valueAxis();
        const double plKey = (havePixel && keyAxis) ? keyAxis->pixelToCoord(pixelPos.x()) : key;
        const double plValue = (havePixel && valueAxis)
            ? valueAxis->pixelToCoord(pixelPos.y())
            : std::numeric_limits<double>::quiet_NaN();

        if (auto* mg = qobject_cast<QCPMultiGraph*>(plottable))
        {
            if (!locator.locateAtKey(mg, plKey, plValue))
                continue;
            const auto& vals = locator.componentValues();

            auto* wfRaw = qobject_cast<QCPWaterfallGraph*>(mg);
            SciQLopWaterfallGraph* wfWrapper = nullptr;
            if (wfRaw)
                wfWrapper = find_waterfall_wrapper(m_plot, wfRaw);

            for (int c = 0; c < mg->componentCount(); ++c)
            {
                const auto& comp = mg->component(c);
                if (!comp.visible)
                    continue;
                double readout;
                if (wfWrapper)
                {
                    readout = wfWrapper->raw_value_at(c, plKey);
                    if (std::isnan(readout))
                        continue;
                }
                else
                {
                    if (c >= vals.size() || std::isnan(vals[c]))
                        continue;
                    readout = vals[c];
                }
                const QString color = comp.pen.color().name();
                lines += QString::fromStdString(
                    fmt::format("<span style='color:{};'>&#9632;</span> {}: <b>{:.6g}</b><br/>",
                        color.toStdString(), comp.name.toStdString(), readout));
            }
        }
        else
        {
            if (!locator.locateAtKey(plottable, plKey, plValue))
                continue;
            if (!std::isnan(locator.data()))
            {
                const QString name = plottable->name();
                lines += QString::fromStdString(
                    fmt::format("&#9632; {}: y=<b>{:.6g}</b> z=<b>{:.6g}</b><br/>",
                        name.toStdString(), locator.value(), locator.data()));
            }
            else if (!std::isnan(locator.value()))
            {
                const QString color = plottable->pen().color().name();
                const QString name = plottable->name();
                lines += QString::fromStdString(
                    fmt::format("<span style='color:{};'>&#9632;</span> {}: <b>{:.6g}</b><br/>",
                        color.toStdString(), name.toStdString(), locator.value()));
            }
        }
    }

    if (lines.isEmpty())
        return {};

    return QString("<b>%1</b><br/>%2").arg(header, lines);
}

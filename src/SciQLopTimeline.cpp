/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Plotables/SciQLopTimeline.hpp"
#include "SciQLopPlots/Plotables/CategoryPalette.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include <stdexcept>

namespace {

std::vector<double> doubles(const SciQLopPyBuffer& b)
{
    if (b.format_code() != 'd')
        throw std::invalid_argument("timeline buffers must be float64");
    return std::vector<double>(b.data(), b.data() + b.flat_size());
}

std::vector<int> remap(const SciQLopPyBuffer& codes, const QVector<int>& table)
{
    std::vector<int> out;
    out.reserve(codes.flat_size());
    for (double code : doubles(codes))
    {
        const auto i = static_cast<qsizetype>(code);
        if (i < 0 || i >= table.size())
            throw std::invalid_argument("lane or category code out of range");
        out.push_back(table[i]);
    }
    return out;
}

} // namespace

SciQLopTimeline::SciQLopTimeline(QCustomPlot* plot, QCPLaneLayout* layout, SciQLopPlotAxis* yAxis,
                                 QVariantMap metaData)
        : SciQLopPlottableInterface(metaData, SciQLopPlots::on_owner_thread(plot, "SciQLopTimeline"))
        , _intervals(new QCPIntervals(plot->xAxis, plot->yAxis, layout))
        , _layout(layout)
        , _y_axis(yAxis)
{
    connect(&CategoryPalette::instance(), &CategoryPalette::changed, this,
            &SciQLopTimeline::_apply_palette);
    connect(layout, &QCPLaneLayout::changed, this, &SciQLopTimeline::lanes_changed);
    connect(layout, &QCPLaneLayout::changed, this, &SciQLopTimeline::replot);
    _apply_palette();
}

SciQLopTimeline::~SciQLopTimeline()
{
    if (_intervals)
        _intervals->parentPlot()->removePlottable(_intervals);
}

void SciQLopTimeline::set_intervals_coded(SciQLopPyBuffer start, SciQLopPyBuffer stop,
                                          SciQLopPyBuffer lane_codes, const QStringList& lane_names,
                                          SciQLopPyBuffer category_codes,
                                          const QStringList& category_names,
                                          const QStringList& labels, SciQLopPyBuffer ids)
{
    SCIQLOP_ON_OWNER_THREAD(set_intervals_coded(start, stop, lane_codes, lane_names,
                                                category_codes, category_names, labels, ids));
    QVector<int> lane_table, category_table;
    for (const auto& name : lane_names)
        lane_table.append(_layout->laneIndex(name));
    for (const auto& name : category_names)
        category_table.append(CategoryPalette::instance().index(name));
    qcp::intervals::Columns columns;
    columns.start = doubles(start);
    columns.stop = doubles(stop);
    columns.lane = remap(lane_codes, lane_table);
    columns.category = remap(category_codes, category_table);
    for (double id : doubles(ids))
        columns.ids.push_back(static_cast<qint64>(id));
    columns.labels = labels;
    _intervals->setData(std::move(columns));
    _apply_palette();
    emit replot();
}

int SciQLopTimeline::count() const { return _intervals ? _intervals->rowCount() : 0; }
QStringList SciQLopTimeline::lanes() const { return _layout->displayOrder(); }

void SciQLopTimeline::set_lanes(const QStringList& lanes)
{
    SCIQLOP_ON_OWNER_THREAD(set_lanes(lanes));
    _layout->setDisplayOrder(lanes);
}

bool SciQLopTimeline::rename_lane(const QString& from, const QString& to)
{
    SciQLopPlots::require_owner_thread(this, "SciQLopTimeline::rename_lane");
    return _layout->renameLane(from, to);
}

int SciQLopTimeline::lane_height() const { return _layout->laneHeight(); }

void SciQLopTimeline::set_lane_height(int px)
{
    SCIQLOP_ON_OWNER_THREAD(set_lane_height(px));
    _layout->setLaneHeight(px);
}

void SciQLopTimeline::set_category_colors(const QMap<QString, QColor>& colors)
{
    SCIQLOP_ON_OWNER_THREAD(set_category_colors(colors));
    for (auto it = colors.begin(); it != colors.end(); ++it)
        CategoryPalette::instance().set_color(it.key(), it.value());
}

QColor SciQLopTimeline::category_color(const QString& category) const
{
    auto& palette = CategoryPalette::instance();
    return palette.colors[palette.index(category)];
}

void SciQLopTimeline::_apply_palette()
{
    if (_intervals)
        _intervals->setCategoryColors(CategoryPalette::instance().colors);
    emit replot();
}

QString SciQLopTimeline::layer() const noexcept
{
    return _intervals ? _intervals->layer()->name() : QString();
}

void SciQLopTimeline::set_visible(bool visible) noexcept
{
    if (_intervals)
        _intervals->setVisible(visible);
    emit replot();
}

bool SciQLopTimeline::visible() const noexcept { return _intervals && _intervals->visible(); }

SciQLopPlotAxisInterface* SciQLopTimeline::y_axis() const noexcept
{
    return _layout && _layout->placement() == QCPLaneLayout::plLanes
               ? static_cast<SciQLopPlotAxisInterface*>(_y_axis)
               : nullptr;
}

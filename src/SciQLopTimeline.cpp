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
#include <QMouseEvent>
#include <algorithm>
#include <stdexcept>

namespace {

const std::pair<const char*, QCPIntervals::EditMode> edit_mode_names[] = {
    { "move", QCPIntervals::emMove },     { "resize", QCPIntervals::emResize },
    { "change_lane", QCPIntervals::emChangeLane }, { "create", QCPIntervals::emCreate },
    { "delete", QCPIntervals::emDelete },
};

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
    connect(plot, &QCustomPlot::mouseMove, this, &SciQLopTimeline::_update_hover);
    plot->installEventFilter(this);
    connect(_intervals, qOverload<const QCPDataSelection&>(&QCPAbstractPlottable::selectionChanged),
            this,
            [this](const QCPDataSelection&)
            {
                emit selected_intervals_changed(selected_ids());
                emit selection_changed(selected());
            });
    connect(_intervals, &QCPIntervals::intervalsEdited, this,
            [this](const QVector<QCPIntervalEdit>& edits)
            {
                QVariantList out;
                const QStringList names = _layout->laneNames();
                for (const auto& e : edits)
                    out.append(QVariant(QVariantList { e.id, e.start, e.stop, names.value(e.lane) }));
                emit intervals_changed(out);
            });
    connect(_intervals, &QCPIntervals::intervalCreated, this,
            [this](double start, double stop, int lane)
            { emit interval_created(start, stop, _layout->laneNames().value(lane)); });
    connect(_intervals, &QCPIntervals::deleteRequested, this,
            [this](const QVector<qint64>& ids) { emit delete_requested(QList<qint64>(ids)); });
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

void SciQLopTimeline::_update_hover(QMouseEvent* event)
{
    const auto hit = _intervals->hitTest(event->pos());
    _set_hovered(hit.row >= 0 && _intervals->realVisibility() ? id_of_row(hit.row) : -1);
}

void SciQLopTimeline::_set_hovered(qint64 id)
{
    if (id == _hovered)
        return;
    _hovered = id;
    emit hovered(id);
}

bool SciQLopTimeline::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Leave)
        _set_hovered(-1);
    return SciQLopPlottableInterface::eventFilter(watched, event);
}

QList<qint64> SciQLopTimeline::selected_ids() const
{
    return _intervals ? QList<qint64>(_intervals->selectedIds()) : QList<qint64>();
}

// simplify: linear scan per id; use a QSet when callers select thousands.
void SciQLopTimeline::select_ids(const QList<qint64>& ids)
{
    SCIQLOP_ON_OWNER_THREAD(select_ids(ids));
    const auto& all = _intervals->columns().ids;
    QVector<int> rows;
    for (int row = 0; row < static_cast<int>(all.size()); ++row)
        if (ids.contains(all[row]))
            rows.append(row);
    _intervals->setSelectedRows(rows);
    emit replot();
}

bool SciQLopTimeline::selected() const noexcept { return _intervals && _intervals->selected(); }

void SciQLopTimeline::set_selected(bool selected) noexcept
{
    SCIQLOP_ON_OWNER_THREAD(set_selected(selected));
    QVector<int> rows;
    if (selected)
        for (int row = 0; row < count(); ++row)
            rows.append(row);
    _intervals->setSelectedRows(rows);
    emit replot();
}

QPointF SciQLopTimeline::pixel_of(double key, const QString& lane) const
{
    return _intervals->pixelOf(key, _layout->laneNames().indexOf(lane));
}

bool SciQLopTimeline::editable() const { return _intervals && _intervals->editable(); }

void SciQLopTimeline::set_editable(bool editable)
{
    SCIQLOP_ON_OWNER_THREAD(set_editable(editable));
    _intervals->setEditable(editable);
}

QStringList SciQLopTimeline::edit_modes() const
{
    QStringList names;
    for (const auto& [name, mode] : edit_mode_names)
        if (_intervals->editModes() & mode)
            names.append(name);
    return names;
}

void SciQLopTimeline::set_edit_modes(const QStringList& names)
{
    SCIQLOP_ON_OWNER_THREAD(set_edit_modes(names));
    QCPIntervals::EditModes modes;
    for (const auto& name : names)
    {
        const auto it = std::ranges::find_if(edit_mode_names,
                                             [&](const auto& m) { return name == m.first; });
        if (it == std::end(edit_mode_names))
            throw std::invalid_argument("unknown edit mode: " + name.toStdString());
        modes |= it->second;
    }
    _intervals->setEditModes(modes);
}

void SciQLopTimeline::set_snap_edges()
{
    SCIQLOP_ON_OWNER_THREAD(set_snap_edges());
    _intervals->setSnap(QCPIntervals::snEdges);
}

void SciQLopTimeline::set_snap_step(double seconds)
{
    SCIQLOP_ON_OWNER_THREAD(set_snap_step(seconds));
    _intervals->setSnap(QCPIntervals::snStep, seconds);
}

void SciQLopTimeline::clear_snap()
{
    SCIQLOP_ON_OWNER_THREAD(clear_snap());
    _intervals->setSnap(QCPIntervals::snNone);
}

QString SciQLopTimeline::snap_mode() const
{
    switch (_intervals->snapMode())
    {
        case QCPIntervals::snEdges:
            return "edges";
        case QCPIntervals::snStep:
            return "step";
        default:
            return "none";
    }
}

double SciQLopTimeline::snap_step() const { return _intervals->snapStep(); }

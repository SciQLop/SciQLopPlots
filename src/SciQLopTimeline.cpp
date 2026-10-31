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
#include <array>
#include "SciQLopPlots/Plotables/CategoryPalette.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include <QMouseEvent>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

// In TimelineEditMode order.
const std::pair<TimelineEditMode, QCPIntervals::EditMode> kEditModes[] = {
    { TimelineEditMode::Move, QCPIntervals::emMove },
    { TimelineEditMode::Resize, QCPIntervals::emResize },
    { TimelineEditMode::ChangeLane, QCPIntervals::emChangeLane },
    { TimelineEditMode::Create, QCPIntervals::emCreate },
    { TimelineEditMode::Delete, QCPIntervals::emDelete },
};

const std::pair<TimelineStack, QCPIntervals::StackMode> kStackModes[] = {
    { TimelineStack::Off, QCPIntervals::skNone },
    { TimelineStack::Time, QCPIntervals::skTime },
    { TimelineStack::Category, QCPIntervals::skCategory },
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
                                 QVariantMap metadata)
        : SciQLopPlottableInterface(metadata, SciQLopPlots::on_owner_thread(plot, "SciQLopTimeline"))
        , _intervals(new QCPIntervals(plot->xAxis, plot->yAxis, layout))
        , _layout(layout)
        , _y_axis(yAxis)
{
    _intervals->setStyle(QCPIntervals::stWave);
    if (objectName().isEmpty())
        setObjectName("timeline");
    connect(&CategoryPalette::instance(), &CategoryPalette::changed, this,
            &SciQLopTimeline::_apply_palette);
    connect(layout, &QCPLaneLayout::changed, this, &SciQLopTimeline::_relay_lanes_changed);
    connect(plot, &QCustomPlot::mouseMove, this, &SciQLopTimeline::_update_hover);
    plot->installEventFilter(this);
    connect(_intervals, qOverload<const QCPDataSelection&>(&QCPAbstractPlottable::selectionChanged),
            this,
            [this](const QCPDataSelection&)
            {
                const QPointer<SciQLopTimeline> self(this);
                emit selected_intervals_changed(selected_ids());
                if (self)
                    _notify_selected(selected());
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
            [this](double start, double stop, int lane, int category)
            {
                const QString laneName = _layout->laneNames().value(lane);
                const QPointer<SciQLopTimeline> self(this);
                emit interval_created(start, stop, laneName);
                if (self)
                    emit interval_created_in(start, stop, laneName,
                                         CategoryPalette::instance().names.value(category));
            });
    connect(_intervals, &QCPIntervals::deleteRequested, this,
            [this](const QVector<qint64>& ids) { emit delete_requested(QList<qint64>(ids)); });
    _apply_palette();
}

void SciQLopTimeline::_relay_lanes_changed()
{
    const QPointer<SciQLopTimeline> self(this);
    emit lanes_changed();
    if (self)
        emit replot();
}

//! NeoQCP emits the lane layout's changed last in each call, so a slot deleting this timeline
//! there runs once NeoQCP is done with the intervals. False when one did.
template <typename F>
bool SciQLopTimeline::_survives(F&& neoqcp_call)
{
    const QPointer<SciQLopTimeline> self(this);
    neoqcp_call();
    return !self.isNull();
}

SciQLopTimeline::~SciQLopTimeline()
{
    // Removing the intervals drops their lane rows, and the layout's changed signal would re-enter
    // this half-destroyed timeline (a slot on lanes_changed or replot may delete it a second time).
    if (_layout)
        disconnect(_layout, nullptr, this, nullptr);
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
    QVector<int> lane_table;
    if (!_survives([&] { lane_table = _layout->laneIndices(lane_names); }))
        return;
    QVector<int> category_table;
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
    // setData() also re-selects by id and emits selectionChanged when one vanished.
    if (!_survives([&] { _intervals->setData(std::move(columns)); }))
        return;
    if (_apply_category_order()) // new names may now have palette indices
        _apply_palette();
}

int SciQLopTimeline::count() const { return _intervals ? _intervals->rowCount() : 0; }

QVariantMap SciQLopTimeline::interval_info(qint64 id) const
{
    const auto rows = _intervals->rowsWithIds({ id });
    if (rows.isEmpty())
        return {};
    const int row = rows.first();
    const auto& c = _intervals->columns();
    const QStringList& categories = CategoryPalette::instance().names;
    return {
        { "id", id },
        { "start", c.start[row] },
        { "stop", c.stop[row] },
        { "duration", c.stop[row] - c.start[row] },
        { "lane", _layout->laneNames().value(c.lane[row]) },
        { "category", categories.value(c.category[row]) },
        { "label", c.labels.value(row) },
    };
}
qint64 SciQLopTimeline::_interval_at(double x, double y) const
{
    const int row = _intervals->hitTest(QPointF(x, y)).row;
    const auto& ids = _intervals->columns().ids;
    return row >= 0 && row < static_cast<int>(ids.size()) ? ids[row] : -1;
}

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

// Guarded after each NeoQCP call: a slot on a signal it emits may delete this timeline.
void SciQLopTimeline::_apply_palette()
{
    const QPointer<SciQLopTimeline> self(this);
    if (_intervals)
        _intervals->setCategoryColors(CategoryPalette::instance().colors);
    if (self && _intervals)
        _intervals->setCategoryNames(CategoryPalette::instance().names);
    if (self)
        emit replot();
}

QString SciQLopTimeline::layer() const noexcept
{
    return _intervals ? _intervals->layer()->name() : QString();
}

void SciQLopTimeline::_set_visible(bool visible)
{
    if (_intervals)
        _intervals->setVisible(visible);
    emit replot();
}

bool SciQLopTimeline::_visible() const { return _intervals && _intervals->visible(); }

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
    const QPointer<SciQLopTimeline> self(this);
    _intervals->setSelectedRows(rows);
    if (self)
        emit replot();
}

bool SciQLopTimeline::_selected() const { return _intervals && _intervals->selected(); }

QColor SciQLopTimeline::_color() const
{
    throw sqp::unsupported_call(unsupported_message("color", typeid(*this).name())
                                + "; use category_color() / set_category_colors()");
}

void SciQLopTimeline::_set_selected(bool selected)
{
    if (!_intervals)
        return;
    QVector<int> rows;
    if (selected)
        for (int row = 0; row < count(); ++row)
            rows.append(row);
    const QPointer<SciQLopTimeline> self(this);
    _intervals->setSelectedRows(rows);
    if (self)
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
    if (editable == this->editable())
        return;
    _intervals->setEditable(editable);
    emit editable_changed(editable);
}

QList<TimelineEditMode> SciQLopTimeline::edit_modes() const
{
    QList<TimelineEditMode> modes;
    for (const auto& [mode, flag] : kEditModes)
        if (_intervals->editModes() & flag)
            modes.append(mode);
    return modes;
}

void SciQLopTimeline::set_edit_modes(const QList<TimelineEditMode>& modes)
{
    SCIQLOP_ON_OWNER_THREAD(set_edit_modes(modes));
    QCPIntervals::EditModes flags;
    for (const auto& [mode, flag] : kEditModes)
        if (modes.contains(mode))
            flags |= flag;
    if (flags == _intervals->editModes())
        return;
    _intervals->setEditModes(flags);
    emit edit_modes_changed(edit_modes());
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
        case QCPIntervals::snTimes:
            return "times";
        default:
            return "none";
    }
}

double SciQLopTimeline::snap_step() const { return _intervals->snapStep(); }

void SciQLopTimeline::set_snap_times(const QList<double>& times)
{
    SCIQLOP_ON_OWNER_THREAD(set_snap_times(times));
    _intervals->setSnapTimes(std::vector<double>(times.cbegin(), times.cend()));
}

QList<double> SciQLopTimeline::snap_times() const
{
    const auto& times = _intervals->snapTimes();
    return QList<double>(times.cbegin(), times.cend());
}

TimelineStack SciQLopTimeline::stack() const
{
    for (const auto& [stack, mode] : kStackModes)
        if (mode == _intervals->stackMode())
            return stack;
    return TimelineStack::Off;
}

void SciQLopTimeline::set_stack(TimelineStack stack)
{
    SCIQLOP_ON_OWNER_THREAD(set_stack(stack));
    if (stack == this->stack())
        return;
    const auto it = std::ranges::find_if(kStackModes, [stack](const auto& m) { return m.first == stack; });
    if (it == std::end(kStackModes))
        return;
    if (!_survives([&] { _intervals->setStackMode(it->second); }))
        return;
    emit stack_changed(stack);
    emit replot();
}

void SciQLopTimeline::set_category_order(const QStringList& names)
{
    SCIQLOP_ON_OWNER_THREAD(set_category_order(names));
    if (names == _category_order)
        return;
    _category_order = names;
    if (!_apply_category_order())
        return;
    emit category_order_changed(names);
    emit replot();
}

//! Names to palette indices; a name no data has used yet has no index and is skipped until then.
//! False when a slot deleted this timeline.
bool SciQLopTimeline::_apply_category_order()
{
    std::vector<int> order;
    for (const auto& name : _category_order)
        if (const int index = CategoryPalette::instance().names.indexOf(name); index >= 0)
            order.push_back(index);
    return _survives([&] { _intervals->setCategoryOrder(std::move(order)); });
}

bool SciQLopTimeline::forbid_overlap() const { return _intervals->forbidOverlap(); }

void SciQLopTimeline::set_forbid_overlap(bool forbid)
{
    SCIQLOP_ON_OWNER_THREAD(set_forbid_overlap(forbid));
    if (forbid == forbid_overlap())
        return;
    _intervals->setForbidOverlap(forbid);
    emit forbid_overlap_changed(forbid);
}

TimelineStyle SciQLopTimeline::style() const
{
    return _intervals->style() == QCPIntervals::stWave ? TimelineStyle::Wave : TimelineStyle::Bars;
}

void SciQLopTimeline::set_style(TimelineStyle style)
{
    SCIQLOP_ON_OWNER_THREAD(set_style(style));
    if (style == this->style())
        return;
    _intervals->setStyle(style == TimelineStyle::Wave ? QCPIntervals::stWave : QCPIntervals::stBars);
    emit style_changed(style);
    emit replot();
}

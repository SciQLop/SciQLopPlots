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
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopTimeSeriesPlot.hpp"
#include "SciQLopPlots/unique_names_factory.hpp"

#include "SciQLopPlots/DragNDrop/PlaceHolderManager.hpp"

#include "SciQLopPlots/Inspector/Inspectors.hpp"
#include "SciQLopPlots/Inspector/Model/Model.hpp"

#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp"
#include "SciQLopPlots/MultiPlots/TimeAxisSynchronizer.hpp"
#include "SciQLopPlots/MultiPlots/VPlotsAlign.hpp"
#include "SciQLopPlots/MultiPlots/XAxisSynchronizer.hpp"

#include <cpp_utils/containers/algorithms.hpp>

#include <QKeyEvent>

SciQLopMultiPlotPanel::SciQLopMultiPlotPanel(QWidget* parent, bool synchronize_x,
                                             bool synchronize_time, Qt::Orientation orientation)
        : SciQLopPlotPanelInterface { parent }, _uuid { QUuid::createUuid() }
{
    _container = new SciQLopPlotContainer(this, orientation);
    connect(_container, &SciQLopPlotContainer::plot_list_changed, this,
            &SciQLopMultiPlotPanel::plot_list_changed);
    connect(_container, &SciQLopPlotContainer::plot_added, this,
            &SciQLopMultiPlotPanel::plot_added);
    connect(_container, &SciQLopPlotContainer::plot_removed, this,
            &SciQLopMultiPlotPanel::plot_removed);
    connect(_container, &SciQLopPlotContainer::plot_inserted, this,
            &SciQLopMultiPlotPanel::plot_inserted);
    connect(_container, &SciQLopPlotContainer::plot_moved, this,
            &SciQLopMultiPlotPanel::plot_moved);

    setWidget(_container);
    this->setWidgetResizable(true);

    // Vertical layout is the default layout where plots are aligned vertically
    // and drag&drop is enabled
    if (orientation == Qt::Vertical)
    {
        ::register_behavior<VPlotsAlign>(_container);
        _place_holder_manager = new PlaceHolderManager(this);
    }
    if (synchronize_x)
        ::register_behavior<XAxisSynchronizer>(_container);
    if (synchronize_time)
    {
        auto b = ::register_behavior<TimeAxisSynchronizer>(_container);
        _default_plot_type = PlotType::TimeSeries;
        connect(b, &TimeAxisSynchronizer::range_changed, this,
                [this](const SciQLopPlotRange& range)
                {
                    this->_container->set_time_axis_range(range);
                    emit this->time_range_changed(range);
                });
    }
    this->setAcceptDrops(true);
    setObjectName(UniqueNamesFactory::unique_name("Panel"));

    if (qobject_cast<SciQLopPlotPanelInterface*>(parent) == nullptr)
        PlotsModel::instance()->addTopLevelNode(this);
}

SciQLopMultiPlotPanel::~SciQLopMultiPlotPanel()
{

}

void SciQLopMultiPlotPanel::replot(bool immediate)
{
    _container->replot(immediate);
}

void SciQLopMultiPlotPanel::add_panel(SciQLopPlotPanelInterface* panel)
{
    _container->addWidget(panel);
    Q_EMIT panel_added(panel);
}

void SciQLopMultiPlotPanel::insert_panel(int index, SciQLopPlotPanelInterface* panel)
{
    _container->insertWidget(index, panel);
    Q_EMIT panel_added(panel);
}

void SciQLopMultiPlotPanel::remove_panel(SciQLopPlotPanelInterface* panel)
{
    _container->removeWidget(panel, true);
    Q_EMIT panel_removed(panel);
}

void SciQLopMultiPlotPanel::move_panel(int from, int to) { }

void SciQLopMultiPlotPanel::add_plot(SciQLopPlotInterface* plot)
{
    _container->add_plot(plot);
}

void SciQLopMultiPlotPanel::remove_plot(SciQLopPlotInterface* plot)
{
    _container->remove_plot(plot);
}

SciQLopPlotInterface* SciQLopMultiPlotPanel::plot_at(int index) const
{
    return _container->plot_at(index);
}

QList<QPointer<SciQLopPlotInterface>> SciQLopMultiPlotPanel::plots() const
{
    return _container->plots();
}

QList<QWidget *> SciQLopMultiPlotPanel::child_widgets() const
{
    return _container->child_widgets();
}

void SciQLopMultiPlotPanel::insert_plot(int index, SciQLopPlotInterface* plot)
{
    _container->insert_plot(index, plot);
}

void SciQLopMultiPlotPanel::move_plot(int from, int to)
{
    _container->move_plot(from, to);
}

void SciQLopMultiPlotPanel::move_plot(SciQLopPlotInterface* plot, int to)
{
    _container->move_plot(plot, to);
}

void SciQLopMultiPlotPanel::clear()
{
    _container->clear();
}

int SciQLopMultiPlotPanel::index(SciQLopPlotInterface* plot) const
{
    return _container->index(plot);
}

int SciQLopMultiPlotPanel::index(const QPointF& pos) const
{
    return _container->index(_container->mapFromParent(pos));
}

int SciQLopMultiPlotPanel::index_from_global_position(const QPointF& pos) const
{
    return _container->index(_container->mapFromGlobal(pos));
}

int SciQLopMultiPlotPanel::indexOf(QWidget* widget) const
{
    return _container->indexOf(widget);
}

bool SciQLopMultiPlotPanel::contains(SciQLopPlotInterface* plot) const
{
    return _container->contains(plot);
}

bool SciQLopMultiPlotPanel::empty() const
{
    return _container->empty();
}

std::size_t SciQLopMultiPlotPanel::size() const
{
    return _container->size();
}

void SciQLopMultiPlotPanel::addWidget(QWidget* widget)
{
    _container->addWidget(widget);
}

void SciQLopMultiPlotPanel::removeWidget(QWidget* widget)
{
    _container->removeWidget(widget, true);
}

SciQLopPlotInterface* SciQLopMultiPlotPanel::create_plot(int index, PlotType plot_type)
{
    SciQLopPlotInterface* plot = nullptr;
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            plot = new SciQLopPlot();
            break;
        case ::PlotType::TimeSeries:
            plot = new SciQLopTimeSeriesPlot();
            break;
        default:
            break;
    }
    if (index == -1)
        add_plot(plot);
    else
        insert_plot(index, plot);
    return plot;
}

void SciQLopMultiPlotPanel::set_x_axis_range(const SciQLopPlotRange& range)
{
    _container->set_x_axis_range(range);
}

const SciQLopPlotRange& SciQLopMultiPlotPanel::x_axis_range() const
{
    return _container->x_axis_range();
}

void SciQLopMultiPlotPanel::set_time_axis_range(const SciQLopPlotRange& range)
{
    if (_container->empty())
        if (auto* time_axis_synchronizer = ::behavior<TimeAxisSynchronizer>(_container))
            time_axis_synchronizer->set_axis_range(range);
    _container->set_time_axis_range(range);
}

const SciQLopPlotRange& SciQLopMultiPlotPanel::time_axis_range() const
{
    return _container->time_axis_range();
}

SciQLopPlotCollectionBehavior*
SciQLopMultiPlotPanel::register_behavior(SciQLopPlotCollectionBehavior* behavior)
{
    return _container->register_behavior(behavior);
}

SciQLopPlotCollectionBehavior* SciQLopMultiPlotPanel::behavior(const QString& type_name) const
{
    return _container->behavior(type_name);
}

void SciQLopMultiPlotPanel::remove_behavior(const QString& type_name)
{
    _container->remove_behavior(type_name);
}

void SciQLopMultiPlotPanel::add_accepted_mime_type(PlotDragNDropCallback* callback)
{
    _accepted_mime_types[callback->mime_type()] = callback;
}

void SciQLopMultiPlotPanel::setSelected(bool selected)
{
    _selected = selected;
    emit selectionChanged(selected);
}

QList<QColor> SciQLopMultiPlotPanel::color_palette() const noexcept
{
    return _container->color_palette();
}

void SciQLopMultiPlotPanel::set_color_palette(const QList<QColor>& palette) noexcept
{
    _container->set_color_palette(palette);
}

QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
SciQLopMultiPlotPanel::plot_impl(const PyBuffer& x, const PyBuffer& y, QStringList labels,
                                 QList<QColor> colors, PlotType plot_type, GraphType graph_type,
                                 GraphMarkerShape marker, int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopGraphInterface>(index, graph_type, x, y, labels,
                                                             colors, marker, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopGraphInterface>(index, graph_type, x, y,
                                                                       labels, colors, marker, metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
SciQLopMultiPlotPanel::plot_impl(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
                                 QString name, bool y_log_scale, bool z_log_scale,
                                 PlotType plot_type, int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopColorMapInterface>(index, GraphType::ColorMap, x, y, z,
                                                                name, y_log_scale, z_log_scale,metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopColorMapInterface>(
                index, GraphType::ColorMap, x, y, z, name, y_log_scale, z_log_scale,metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
SciQLopMultiPlotPanel::plot_impl(const QList<PyBuffer>& values, QStringList labels,
                                 QList<QColor> colors, GraphMarkerShape marker, int index, QVariantMap metaData)
{
    auto* plot = new SciQLopNDProjectionPlot();
    if (index == -1)
        add_plot(plot);
    else
        insert_plot(index, plot);

    return { plot, plot->parametric_curve(values, labels, colors,marker, metaData) };
}

QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> SciQLopMultiPlotPanel::plot_impl(
    GetDataPyCallable callable, QStringList labels, QList<QColor> colors, GraphType graph_type,
    GraphMarkerShape marker, PlotType plot_type, QObject* sync_with, int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopGraphInterface>(index, graph_type, callable, labels,
                                                             colors, marker, sync_with, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopGraphInterface>(
                index, graph_type, callable, labels, colors, marker, sync_with, metaData);
            break;
        case ::PlotType::Projections:
            return _plot<SciQLopNDProjectionPlot, SciQLopGraphInterface>(
                index, graph_type, callable, labels, colors, marker, sync_with, metaData);
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
SciQLopMultiPlotPanel::plot_impl(GetDataPyCallable callable, QString name, bool y_log_scale,
                                 bool z_log_scale, PlotType plot_type, QObject* sync_with,
                                 int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopColorMapInterface>(
                index, GraphType::ColorMap, callable, name, y_log_scale, z_log_scale, sync_with, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopColorMapInterface>(
                index, GraphType::ColorMap, callable, name, y_log_scale, z_log_scale, sync_with, metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

void SciQLopMultiPlotPanel::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_O:
            _container->organize_plots();
            event->accept();
            break;
        default:
            break;
    }
    if (!event->isAccepted())
        QScrollArea::keyPressEvent(event);
}

void SciQLopMultiPlotPanel::dragEnterEvent(QDragEnterEvent* event)
{
    using namespace cpp_utils;
    const auto formats = event->mimeData()->formats();
    for (const auto& format : formats)
    {
        if (containers::contains(_accepted_mime_types, format))
        {
            event->acceptProposedAction();
            _current_callback = _accepted_mime_types[format];
            if (_current_callback->create_placeholder())
                _place_holder_manager->dragEnterEvent(event);
            return;
        }
    }
    _current_callback = nullptr;
}

void SciQLopMultiPlotPanel::dragMoveEvent(QDragMoveEvent* event)
{
    if (_current_callback)
    {
        if (_current_callback->create_placeholder())
            _place_holder_manager->dragMoveEvent(event);
        event->acceptProposedAction();
    }
}

void SciQLopMultiPlotPanel::dragLeaveEvent(QDragLeaveEvent* event)
{
    if (_current_callback && _current_callback->create_placeholder())
        _place_holder_manager->dragLeaveEvent(event);
    _current_callback = nullptr;
}

void SciQLopMultiPlotPanel::dropEvent(QDropEvent* event)
{
    if (_current_callback)
    {
        SciQLopPlotInterface* drop_plot = nullptr;
        auto drop_result = _place_holder_manager->dropEvent(event);
        if (drop_result.location == DropLocation::NewPlot)
        {
            drop_plot = create_plot(drop_result.index, _default_plot_type);
        }
        else
        {
            drop_plot = plot_at(drop_result.index);
        }

        event->acceptProposedAction();
        if (drop_plot)
            _current_callback->call(drop_plot, event->mimeData());
        _current_callback = nullptr;
    }
}

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

#include "SciQLopPlots/SciQLopPlot.hpp"

#include "SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotPanelInterface.hpp"

SciQLopPlotContainer::SciQLopPlotContainer(QWidget* parent, Qt::Orientation orientation)
        : QSplitter(orientation, parent)
{
    if (orientation == Qt::Vertical)
        this->setLayout(new QVBoxLayout(this));
    else
        this->setLayout(new QHBoxLayout(this));
    this->setContentsMargins(0, 0, 0, 0);
    this->layout()->setContentsMargins(0, 0, 0, 0);
    this->layout()->setSpacing(0);
    this->setChildrenCollapsible(false);
    setProperty("empty", true);
}

SciQLopPlotContainer::~SciQLopPlotContainer() { }

void SciQLopPlotContainer::insertWidget(int index, QWidget* widget)
{
    QSplitter::insertWidget(index, widget);
    if (auto* plot = qobject_cast<SciQLopPlotInterface*>(widget))
    {
        if (plot->color_palette().empty())
            plot->set_color_palette(_color_palette);
        emit plot_list_changed(plots());
        emit plot_added(plot);
        emit plot_inserted(plot, index);
        connect(
            plot, &SciQLopPlotInterface::destroyed, this, [this, plot]() { remove_plot(plot); },
            Qt::QueuedConnection);
    }
    else if(auto* panel = qobject_cast<SciQLopPlotPanelInterface*>(widget))
    {
        emit panel_added(panel);
        emit panel_inserted(panel, index);
    }
}

void SciQLopPlotContainer::addWidget(QWidget* widget)
{
    insertWidget(QSplitter::count(), widget);
}

void SciQLopPlotContainer::insert_plot(int index, SciQLopPlotInterface* plot)
{
    insertWidget(index, plot);
}

void SciQLopPlotContainer::add_plot(SciQLopPlotInterface* plot)
{
    addWidget(plot);
}

void SciQLopPlotContainer::move_plot(int from, int to)
{
    move_plot(plot_at(from), to);
}

void SciQLopPlotContainer::move_plot(SciQLopPlotInterface* plot, int to)
{
    QSplitter::insertWidget(to, plot);
    emit plot_list_changed(plots());
    emit plot_moved(plot, to);
}

void SciQLopPlotContainer::remove_plot(SciQLopPlotInterface* plot)
{
    remove_plot(plot, true);
}

void SciQLopPlotContainer::remove_plot(SciQLopPlotInterface* plot, bool destroy)
{
    if (indexOf(plot) != -1)
    {
        plot->setParent(nullptr);
        if (destroy)
            plot->deleteLater();
        if (empty())
            setProperty("empty", true);
        emit plot_list_changed(plots());
        emit plot_removed(plot);
    }
}

void SciQLopPlotContainer::removeWidget(QWidget* widget, bool destroy)
{
    if (auto* plot = qobject_cast<SciQLopPlotInterface*>(widget); plot)
        remove_plot(plot, destroy);
    else
    {
        widget->setParent(nullptr);
        if (destroy)
            widget->deleteLater();
    }
}

void SciQLopPlotContainer::clear()
{
    while (QSplitter::count())
    {
        auto* widget = this->widget(0);
        widget->setParent(nullptr);
        widget->deleteLater();
    }
    setProperty("empty", true);
    emit plot_list_changed({});
}

QList<QWidget *> SciQLopPlotContainer::child_widgets() const
{
    QList<QWidget*> widgets;
    for (int i = 0; i < count(); i++)
    {
        widgets.append(widget(i));
    }
    return widgets;
}

SciQLopPlotCollectionBehavior*
SciQLopPlotContainer::register_behavior(SciQLopPlotCollectionBehavior* behavior)
{
    behavior->setParent(this);
    _behaviors[behavior->metaObject()->className()] = behavior;
    behavior->updatePlotList(plots());
    connect(this, &SciQLopPlotContainer::plot_list_changed, behavior,
            &SciQLopPlotCollectionBehavior::updatePlotList);
    connect(this, &SciQLopPlotContainer::plot_added, behavior,
            &SciQLopPlotCollectionBehavior::plotAdded);
    connect(this, &SciQLopPlotContainer::plot_removed, behavior,
            &SciQLopPlotCollectionBehavior::plotRemoved);

    connect(this, &SciQLopPlotContainer::panel_added, behavior,
            &SciQLopPlotCollectionBehavior::panelAdded);
    connect(this, &SciQLopPlotContainer::panel_removed, behavior,
            &SciQLopPlotCollectionBehavior::panelRemoved);

    return behavior;
}

SciQLopPlotCollectionBehavior* SciQLopPlotContainer::behavior(const QString& type_name) const
{
    return _behaviors.value(type_name, nullptr);
}

void SciQLopPlotContainer::remove_behavior(const QString& type_name)
{
    if (_behaviors.contains(type_name))
    {
        disconnect(this, &SciQLopPlotContainer::plot_list_changed, _behaviors[type_name],
                   &SciQLopPlotCollectionBehavior::updatePlotList);
        delete _behaviors[type_name];
        _behaviors.remove(type_name);
    }
}

void SciQLopPlotContainer::organize_plots()
{
    auto _sizes = sizes();
    const auto total_height = std::accumulate(std::cbegin(_sizes), std::cend(_sizes), 0);
    const auto per_widget_height = total_height / std::size(_sizes);
    std::transform(std::cbegin(_sizes), std::cend(_sizes), std::begin(_sizes),
                   [per_widget_height](int height) { return per_widget_height; });
    setSizes(_sizes);
}

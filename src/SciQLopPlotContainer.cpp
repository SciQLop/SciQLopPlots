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

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"

#include "SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotPanelInterface.hpp"

#include <QCoreApplication>
#include <QPointer>

#include <algorithm>

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

SciQLopPlotContainer::~SciQLopPlotContainer()
{
    // Deleting a plot can run the event loop (the browser does, waiting for its worker threads):
    // the calls the plots queued for this container must not reach it half-destroyed.
    for (const auto& plot : plots())
        disconnect(plot, nullptr, this, nullptr);
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
}

void SciQLopPlotContainer::insertWidget(int index, QWidget* widget)
{
    sqp::require_object(widget, "insert: widget");
    // A panel inside itself, or inside a panel it holds, is a widget cycle: Qt loops on it.
    if (widget == this || widget->isAncestorOf(this))
        throw std::invalid_argument("insert: a panel can't hold itself or a panel that holds it");
    QSplitter::insertWidget(index, widget);
    if (auto* plot = qobject_cast<SciQLopPlotInterface*>(widget))
    {
        if (plot->color_palette().empty())
            plot->set_color_palette(_color_palette);
        // A destroyed plot has already left the splitter. Not remove_plot(plot): by the time
        // a queued call runs, a new plot may live at that address, and it would be deleted.
        connect(
            plot, &SciQLopPlotInterface::destroyed, this,
            [this]() { emit plot_list_changed(plots()); }, Qt::QueuedConnection);
        // A timeline follows its natural height until the user makes it taller.
        if (auto* sp = qobject_cast<SciQLopPlot*>(plot))
            connect(sp, &SciQLopPlot::natural_height_changed, this,
                    [this, sp](int previous, int natural)
                    {
                        if (previous == 0 || sp->height() <= previous)
                            set_widget_height(sp, natural);
                    });
        announce_plot(plot, index);
    }
    else if (auto* panel = qobject_cast<SciQLopPlotPanelInterface*>(widget))
        announce_panel(panel, index);
}

// Last thing insertWidget does: a slot may delete this container (its panel) at any of these.
void SciQLopPlotContainer::announce_plot(SciQLopPlotInterface* plot, int index)
{
    QPointer<SciQLopPlotContainer> alive(this);
    QPointer<SciQLopPlotInterface> added(plot); // a slot may delete the plot itself, too
    emit plot_joined(plot);
    if (alive && added)
        emit plot_list_changed(plots());
    if (alive && added)
        emit plot_added(plot);
    if (alive && added)
        emit plot_inserted(plot, index);
}

void SciQLopPlotContainer::announce_panel(SciQLopPlotPanelInterface* panel, int index)
{
    QPointer<SciQLopPlotContainer> alive(this);
    QPointer<SciQLopPlotPanelInterface> added(panel);
    emit panel_added(panel);
    if (alive && added)
        emit panel_inserted(panel, index);
}

void SciQLopPlotContainer::addWidget(QWidget* widget)
{
    insertWidget(QSplitter::count(), widget);
}

// A plot index counts plots only, as plot_at does: the widget position of the plot at \a index, or
// the end past the last plot. Nested panels sit among the plots in the splitter.
int SciQLopPlotContainer::widget_position(int index) const
{
    if (auto* plot = index >= 0 ? plot_at(index) : nullptr)
        return indexOf(plot);
    return QSplitter::count();
}

void SciQLopPlotContainer::insert_plot(int index, SciQLopPlotInterface* plot)
{
    insertWidget(widget_position(index), plot);
}

void SciQLopPlotContainer::add_plot(SciQLopPlotInterface* plot)
{
    addWidget(plot);
}

void SciQLopPlotContainer::move_plot(int from, int to)
{
    auto* plot = plot_at(from);
    if (plot == nullptr)
        throw std::invalid_argument("move_plot: no plot at index " + std::to_string(from) + " (the panel has "
                                    + std::to_string(size()) + ")");
    move_plot(plot, to);
}

void SciQLopPlotContainer::move_plot(SciQLopPlotInterface* plot, int to)
{
    if (indexOf(sqp::require_object(plot, "move_plot: plot")) == -1)
        throw std::invalid_argument("move_plot: this plot is not in the panel");
    // Negative or past the last plot (Qt's "append"): the last plot's position, which this plot takes
    // as the others move up.
    const int last = static_cast<int>(plots().size()) - 1;
    QSplitter::insertWidget(widget_position(to < 0 ? last : std::min(to, last)), plot);
    // A slot on the first signal may delete this container (its panel).
    const auto now = plots();
    const auto landed = static_cast<int>(now.indexOf(plot)); // where it went, not what was asked
    QPointer<SciQLopPlotContainer> alive(this);
    emit plot_list_changed(now);
    if (alive)
        emit plot_moved(plot, landed);
}

void SciQLopPlotContainer::move_panel(int from, int to)
{
    auto* panel = qobject_cast<SciQLopPlotPanelInterface*>(QSplitter::widget(from));
    if (panel == nullptr)
        throw std::invalid_argument("move_panel: no panel at index " + std::to_string(from));
    move_panel(panel, to);
}

void SciQLopPlotContainer::move_panel(SciQLopPlotPanelInterface* panel, int to)
{
    if (indexOf(sqp::require_object(panel, "move_panel: panel")) == -1)
        throw std::invalid_argument("move_panel: this panel is not in the panel");
    QSplitter::insertWidget(to, panel);
    emit panel_moved(panel, to);
}

void SciQLopPlotContainer::remove_plot(SciQLopPlotInterface* plot)
{
    remove_plot(plot, true);
}

void SciQLopPlotContainer::remove_plot(SciQLopPlotInterface* plot, bool destroy)
{
    sqp::require_object(plot, "remove_plot: plot");
    if (indexOf(plot) != -1)
    {
        plot->setParent(nullptr);
        if (destroy)
            plot->deleteLater();
        if (empty())
            setProperty("empty", true);
        // A slot on plot_removed may delete this container (its panel).
        QPointer<SciQLopPlotContainer> alive(this);
        emit plot_removed(plot);
        if (!alive)
            return;
    }
    emit plot_list_changed(plots());
}

void SciQLopPlotContainer::removeWidget(QWidget* widget, bool destroy)
{
    sqp::require_object(widget, "remove: widget");
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

QList<QWidget*> SciQLopPlotContainer::child_widgets() const
{
    QList<QWidget*> widgets;
    for (int i = 0; i < count(); i++)
    {
        widgets.append(widget(i));
    }
    return widgets;
}

void SciQLopPlotContainer::export_paint(QPainter* painter, const QRect& target,
                                        SciQLopExportTarget kind)
{
    export_children(this, child_widgets(), painter, target, kind);
}

std::size_t SciQLopPlotContainer::content_height() const
{
    return this->height()
        - (this->layout()->contentsMargins().top() + this->layout()->contentsMargins().bottom());
}

std::size_t SciQLopPlotContainer::content_width() const
{
    return this->width()
        - (this->layout()->contentsMargins().left() + this->layout()->contentsMargins().right());
}

SciQLopPlotCollectionBehavior*
SciQLopPlotContainer::register_behavior(SciQLopPlotCollectionBehavior* behavior)
{
    sqp::require_object(behavior, "register_behavior: behavior");
    behavior->setParent(this);
    _behaviors[behavior->metaObject()->className()] = behavior;
    behavior->updatePlotList(plots());
    connect(this, &SciQLopPlotContainer::plot_list_changed, behavior,
            &SciQLopPlotCollectionBehavior::updatePlotList);
    connect(this, &SciQLopPlotContainer::plot_joined, behavior,
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

namespace
{
//! Timeline plots (Minimum policy) keep their natural height; the others share the rest.
bool keeps_natural_height(const QWidget* w)
{
    return w->sizePolicy().verticalPolicy() == QSizePolicy::Minimum;
}

//! Heights for \a total pixels: a widget with a \a natural height gets it, the others share
//! what is left by \a weights. If the natural heights leave no room, everyone shares equally.
QList<int> distribute(int total, const QList<int>& natural, const QList<int>& weights)
{
    if (total <= 0)
        return QList<int>(natural.size(), 0);
    const int reserved = std::accumulate(natural.cbegin(), natural.cend(), 0);
    const int total_weight = std::accumulate(weights.cbegin(), weights.cend(), 0);
    if (total_weight == 0 || reserved >= total)
        return distribute(total, QList<int>(natural.size(), 0), QList<int>(natural.size(), 1));
    QList<int> heights;
    for (int i = 0; i < natural.size(); ++i)
        heights.append(natural[i] + (total - reserved) * weights[i] / total_weight);
    heights.last() += total - std::accumulate(heights.cbegin(), heights.cend(), 0);
    return heights;
}
}

void SciQLopPlotContainer::set_widget_height(QWidget* widget, int height)
{
    auto heights = sizes();
    const int i = indexOf(widget);
    if (i < 0 || heights.size() < 2)
        return;
    const int total = std::accumulate(heights.cbegin(), heights.cend(), 0);
    QList<int> natural(heights.size(), 0), weights(heights);
    natural[i] = height;
    weights[i] = 0;
    setSizes(distribute(total, natural, weights));
}

void SciQLopPlotContainer::organize_plots()
{
    const auto current = sizes();
    if (current.isEmpty())
        return;
    QList<int> natural, weights;
    for (int i = 0; i < current.size(); ++i)
    {
        const bool keeps = keeps_natural_height(widget(i));
        natural.append(keeps ? widget(i)->sizeHint().height() : 0);
        weights.append(keeps ? 0 : std::max(1, int(widget(i)->sizePolicy().verticalStretch())));
    }
    setSizes(distribute(std::accumulate(current.cbegin(), current.cend(), 0), natural, weights));
}

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
#include "SciQLopPlots/Inspector/Model/TypeDescriptor.hpp"
#include "SciQLopPlots/Inspector/Model/DelegateRegistry.hpp"

// Domain types
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"
#include "SciQLopPlots/Plotables/SciQLopHistogram2D.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

// Delegate widgets
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopMultiPlotPanelDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphComponentDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotAxisDelegate.hpp"

namespace
{

void register_all_types()
{
    auto& types = TypeRegistry::instance();

    types.register_type<SciQLopMultiPlotPanel>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto panel = qobject_cast<SciQLopMultiPlotPanel*>(obj);
            QList<QObject*> result;
            if (panel)
                for (auto w : panel->child_widgets())
                    result.append(w);
            return result;
        },
        .connect_children = [](QObject* obj, auto add, auto remove)
                -> QList<QMetaObject::Connection> {
            auto panel = qobject_cast<SciQLopMultiPlotPanel*>(obj);
            if (!panel) return {};
            return {
                QObject::connect(panel, &SciQLopMultiPlotPanel::plot_added, panel,
                    [add](SciQLopPlotInterface* p) { add(p); }),
                QObject::connect(panel, &SciQLopMultiPlotPanel::plot_removed, panel,
                    [remove](SciQLopPlotInterface* p) { remove(p); }),
                QObject::connect(panel, &SciQLopMultiPlotPanel::panel_added, panel,
                    [add](SciQLopPlotPanelInterface* p) { add(p); }),
                QObject::connect(panel, &SciQLopMultiPlotPanel::panel_removed, panel,
                    [remove](SciQLopPlotPanelInterface* p) { remove(p); }),
            };
        },
        .set_selected = [](QObject* obj, bool s) {
            if (auto p = qobject_cast<SciQLopMultiPlotPanel*>(obj))
                p->setSelected(s);
        },
    });

    // graph_list_changed() has no args — can't do individual add/remove.
    // TODO: add graph_added/graph_removed signals to SciQLopPlotInterface
    types.register_type<SciQLopPlot>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto plot = qobject_cast<SciQLopPlot*>(obj);
            if (!plot) return {};
            QList<QObject*> c;
            c.append(plot->x_axis());
            c.append(plot->y_axis());
            if (plot->has_colormap()) {
                c.append(plot->z_axis());
                c.append(plot->y2_axis());
            }
            for (auto p : plot->plottables())
                c.append(p);
            return c;
        },
        .connect_children = [](QObject* obj, auto add, auto /*remove*/)
                -> QList<QMetaObject::Connection> {
            auto plot = qobject_cast<SciQLopPlot*>(obj);
            if (!plot) return {};
            return {
                QObject::connect(plot, &SciQLopPlot::graph_list_changed, plot,
                    [plot, add]() {
                        for (auto p : plot->plottables())
                            add(p);
                    }),
            };
        },
        .set_selected = [](QObject* obj, bool s) {
            if (auto p = qobject_cast<SciQLopPlot*>(obj))
                p->set_selected(s);
        },
        .flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable
               | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled,
    });

    // Same limitation: graph_list_changed has no args
    types.register_type<SciQLopNDProjectionPlot>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto plot = qobject_cast<SciQLopNDProjectionPlot*>(obj);
            if (!plot) return {};
            QList<QObject*> c;
            for (auto p : plot->plottables())
                c.append(p);
            return c;
        },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto p = qobject_cast<SciQLopNDProjectionPlot*>(obj))
                p->set_selected(s);
        },
    });

    // SciQLopNDProjectionCurves inherits this via metaObject chain lookup
    types.register_type<SciQLopGraphInterface>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto graph = qobject_cast<SciQLopGraphInterface*>(obj);
            if (!graph) return {};
            QList<QObject*> c;
            for (auto comp : graph->components())
                c.append(comp);
            return c;
        },
        .connect_children = [](QObject* obj, auto add, auto /*remove*/)
                -> QList<QMetaObject::Connection> {
            auto graph = qobject_cast<SciQLopGraphInterface*>(obj);
            if (!graph) return {};
            return {
                QObject::connect(graph, &SciQLopGraphInterface::component_list_changed, graph,
                    [graph, add]() {
                        for (auto comp : graph->components())
                            add(comp);
                    }),
            };
        },
        .set_selected = [](QObject* obj, bool s) {
            if (auto g = qobject_cast<SciQLopGraphInterface*>(obj);
                g && g->selected() != s)
                g->set_selected(s);
        },
    });

    types.register_type<SciQLopGraphComponentInterface>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto c = qobject_cast<SciQLopGraphComponentInterface*>(obj))
                c->set_selected(s);
        },
        .deletable = false,
    });

    types.register_type<SciQLopColorMap>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto cm = qobject_cast<SciQLopColorMap*>(obj))
                cm->set_selected(s);
        },
    });

    types.register_type<SciQLopHistogram2D>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto h = qobject_cast<SciQLopHistogram2D*>(obj))
                h->set_selected(s);
        },
    });

    types.register_type<SciQLopPlotAxisInterface>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto a = qobject_cast<SciQLopPlotAxisInterface*>(obj))
                a->set_selected(s);
        },
        .deletable = false,
    });

    auto& delegates = DelegateRegistry::instance();
    delegates.register_type<SciQLopMultiPlotPanelDelegate>();
    delegates.register_type<SciQLopPlotDelegate>();
    delegates.register_type<SciQLopGraphDelegate>();
    delegates.register_type<SciQLopGraphComponentDelegate>();
    delegates.register_type<SciQLopColorMapDelegate>();
    delegates.register_type<SciQLopWaterfallDelegate>();
    delegates.register_type<SciQLopPlotAxisDelegate>();
}

static const int _registrations = (register_all_types(), 0);

} // anonymous namespace

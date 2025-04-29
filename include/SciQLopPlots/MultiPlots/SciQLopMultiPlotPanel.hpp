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
#pragma once
#include "SciQLopPlots/DragNDrop/PlotDragNDropCallback.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotPanelInterface.hpp"

#include <QGridLayout>
#include <QScrollArea>
#include <QUuid>
#include <QWidget>
#include <map>

class SciQLopPlotContainer;
class SciQLopPlot;
class PlaceHolderManager;

class SciQLopMultiPlotPanel : public SciQLopPlotPanelInterface
{
    Q_OBJECT

    SciQLopPlotContainer* _container = nullptr;
    std::map<QString, PlotDragNDropCallback*> _accepted_mime_types;
    PlotDragNDropCallback* _current_callback = nullptr;
    PlaceHolderManager* _place_holder_manager = nullptr;
    PlotType _default_plot_type = PlotType::BasicXY;
    QUuid _uuid;
    bool _selected = false;

protected:
    template <typename T, GraphType graph_type, typename... Args>
    auto __plot(T* plot, Args&&... args)
        -> decltype(std::declval<T*>()->line(std::forward<Args>(args)...))
    {
        if constexpr (graph_type == GraphType::Line)
            return plot->line(std::forward<Args>(args)...);
        if constexpr (graph_type == GraphType::ParametricCurve)
            return plot->parametric_curve(std::forward<Args>(args)...);
        if constexpr (graph_type == GraphType::Scatter)
            return plot->scatter(std::forward<Args>(args)...);
        return nullptr;
    }

    template <typename T, GraphType graph_type, typename... Args>
    auto __plot(T* plot, Args&&... args)
        -> decltype(std::declval<T*>()->colormap(std::forward<Args>(args)...))
    {
        if constexpr (graph_type == GraphType::ColorMap)
            return plot->colormap(std::forward<Args>(args)...);
        return nullptr;
    }

    template <typename T, typename U, typename... Args>
    QPair<T*, U*> _plot(int index, GraphType graph_type, Args&&... args)
    {
        auto* plot = new T();
        if (index == -1)
            add_plot(plot);
        else
            insert_plot(index, plot);

        if (graph_type == GraphType::Line)
        {
            return { plot, __plot<T, GraphType::Line>(plot, std::forward<Args>(args)...) };
        }

        if (graph_type == GraphType::Scatter)
        {
            return { plot, __plot<T, GraphType::Scatter>(plot, std::forward<Args>(args)...) };
        }

        if (graph_type == GraphType::ParametricCurve)
        {
            return { plot,
                     __plot<T, GraphType::ParametricCurve>(plot, std::forward<Args>(args)...) };
        }
        if (graph_type == GraphType::ColorMap)
        {
            return { plot, __plot<T, GraphType::ColorMap>(plot, std::forward<Args>(args)...) };
        }
        return { nullptr, nullptr };
    }

    virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    plot_impl(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
              ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              int index = -1, QVariantMap metaData={}) Q_DECL_OVERRIDE;

    virtual QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
    plot_impl(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
              QString name = QStringLiteral("ColorMap"), bool y_log_scale = false,
              bool z_log_scale = false, ::PlotType plot_type = ::PlotType::BasicXY,
              int index = -1, QVariantMap metaData={}) Q_DECL_OVERRIDE;

    virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    plot_impl(const QList<PyBuffer>& values, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(),
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              int index = -1, QVariantMap metaData={}) Q_DECL_OVERRIDE;

    virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    plot_impl(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr,
              int index = -1, QVariantMap metaData={}) Q_DECL_OVERRIDE;

    virtual QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
    plot_impl(GetDataPyCallable callable, QString name = QStringLiteral("ColorMap"),
              bool y_log_scale = false, bool z_log_scale = false,
              ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr,
              int index = -1, QVariantMap metaData={}) Q_DECL_OVERRIDE;

public:
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectionChanged FINAL);
    Q_PROPERTY(bool empty READ empty FINAL);

    SciQLopMultiPlotPanel(QWidget* parent = nullptr, bool synchronize_x = true,
                          bool synchronize_time = false,
                          Qt::Orientation orientation = Qt::Vertical);

    ~SciQLopMultiPlotPanel() override;

    virtual void replot(bool immediate = false) Q_DECL_OVERRIDE;

    inline QUuid uuid() const { return _uuid; }

    virtual void add_panel(SciQLopPlotPanelInterface* panel) Q_DECL_OVERRIDE;
    virtual void insert_panel(int index, SciQLopPlotPanelInterface* panel) Q_DECL_OVERRIDE;
    virtual void remove_panel(SciQLopPlotPanelInterface* panel) Q_DECL_OVERRIDE;
    virtual void move_panel(int from, int to) Q_DECL_OVERRIDE;

    virtual inline void move_panel(SciQLopPlotPanelInterface* panel, int to) Q_DECL_OVERRIDE
    {
        WARN_ABSTRACT_METHOD
    }

    void add_plot(SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    void remove_plot(SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    SciQLopPlotInterface* plot_at(int index) const Q_DECL_OVERRIDE;
    QList<QPointer<SciQLopPlotInterface>> plots() const Q_DECL_OVERRIDE;

    virtual QList<QWidget*> child_widgets() const Q_DECL_OVERRIDE;

    virtual void insert_plot(int index, SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    virtual void move_plot(int from, int to) Q_DECL_OVERRIDE;
    virtual void move_plot(SciQLopPlotInterface* plot, int to) Q_DECL_OVERRIDE;
    void clear() Q_DECL_OVERRIDE;

    int index(SciQLopPlotInterface* plot) const Q_DECL_OVERRIDE;
    int index(const QPointF& pos) const Q_DECL_OVERRIDE;
    int index_from_global_position(const QPointF& pos) const Q_DECL_OVERRIDE;
    int indexOf(QWidget* widget) const;


    bool contains(SciQLopPlotInterface* plot) const Q_DECL_OVERRIDE;

    bool empty() const Q_DECL_OVERRIDE;
    std::size_t size() const Q_DECL_OVERRIDE;

    void addWidget(QWidget* widget);
    void removeWidget(QWidget* widget);

    SciQLopPlotInterface* create_plot(int index = -1, PlotType plot_type = PlotType::BasicXY);


    void set_x_axis_range(const SciQLopPlotRange& range) Q_DECL_OVERRIDE;
    const SciQLopPlotRange& x_axis_range() const Q_DECL_OVERRIDE;

    void set_time_axis_range(const SciQLopPlotRange& range) Q_DECL_OVERRIDE;
    const SciQLopPlotRange& time_axis_range() const Q_DECL_OVERRIDE;

    inline void set_x_axis_range(double start, double stop)
    {
        set_x_axis_range(SciQLopPlotRange(start, stop));
    }

    inline void set_time_axis_range(double start, double stop)
    {
        set_time_axis_range(SciQLopPlotRange(start, stop));
    }

    inline void set_time_axis_range(const QDateTime& start, const QDateTime& stop)
    {
        set_time_axis_range(SciQLopPlotRange(start, stop));
    }

    SciQLopPlotCollectionBehavior*
    register_behavior(SciQLopPlotCollectionBehavior* behavior) Q_DECL_OVERRIDE;
    SciQLopPlotCollectionBehavior* behavior(const QString& type_name) const Q_DECL_OVERRIDE;
    void remove_behavior(const QString& type_name) Q_DECL_OVERRIDE;

    void add_accepted_mime_type(PlotDragNDropCallback* callback);

    inline bool selected() const { return _selected; }

    void setSelected(bool selected);

    virtual QList<QColor> color_palette() const noexcept override;

    virtual void set_color_palette(const QList<QColor>& palette) noexcept override;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

public:
#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void panel_added(SciQLopPlotPanelInterface* panel);
    Q_SIGNAL void panel_removed(SciQLopPlotPanelInterface* panel);
};

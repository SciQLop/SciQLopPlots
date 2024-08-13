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
#include "SciQLopPlotCollection.hpp"

#include <QGridLayout>
#include <QScrollArea>
#include <QWidget>

#include "qcustomplot.h"


class SciQLopPlotContainer;
class SciQLopPlot;

class SciQLopMultiPlotPanel : public QScrollArea, public SciQLopPlotCollectionInterface
{
    Q_OBJECT
    SciQLopPlotContainer* _container = nullptr;

protected:
    template <typename T, GraphType graph_type, typename... Args>
    auto __plot(T* plot,
        Args&&... args) -> decltype(std::declval<T*>()->line(std::forward<Args>(args)...), void())
    {
        if constexpr (graph_type == GraphType::Line)
            plot->line(std::forward<Args>(args)...);
        if constexpr (graph_type == GraphType::ParametricCurve)
            plot->parametric_curve(std::forward<Args>(args)...);
    }

    template <typename T, GraphType graph_type, typename... Args>
    auto __plot(T* plot,
        Args&&... args) -> decltype(std::declval<T*>()->colormap(std::forward<Args>(args)...),
                            void())
    {
        if constexpr (graph_type == GraphType::ColorMap)
            plot->colormap(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    T* _plot(int index, GraphType graph_type, Args&&... args)
    {
        auto* plot = new T();
        if (index == -1)
            addPlot(plot);
        else
            insertPlot(index, plot);
        if (graph_type == GraphType::Line)
            __plot<T, GraphType::Line>(plot, std::forward<Args>(args)...);
        if (graph_type == GraphType::ParametricCurve)
            __plot<T, GraphType::ParametricCurve>(plot, std::forward<Args>(args)...);
        if (graph_type == GraphType::ColorMap)
            __plot<T, GraphType::ColorMap>(plot, std::forward<Args>(args)...);
        return plot;
    }

    virtual SciQLopPlotInterface* plot_impl(const PyBuffer& x, const PyBuffer& y,
        QStringList labels = QStringList(), QList<QColor> colors = QList<QColor>(),
        ::DataOrder data_order = ::DataOrder::RowMajor, ::PlotType plot_type = ::PlotType::BasicXY,
        ::GraphType graph_type = ::GraphType::Line, int index = -1) Q_DECL_OVERRIDE;

    virtual SciQLopPlotInterface* plot_impl(const PyBuffer& x, const PyBuffer& y,
        const PyBuffer& z, QString name = QStringLiteral("ColorMap"),
        ::DataOrder data_order = ::DataOrder::RowMajor, bool y_log_scale = false,
        bool z_log_scale = false, ::PlotType plot_type = ::PlotType::BasicXY,
        int index = -1) Q_DECL_OVERRIDE;

    virtual SciQLopPlotInterface* plot_impl(GetDataPyCallable callable,
        QStringList labels = QStringList(), QList<QColor> colors = QList<QColor>(),
        ::DataOrder data_order = ::DataOrder::RowMajor, ::GraphType graph_type = ::GraphType::Line,
        ::PlotType plot_type = ::PlotType::BasicXY, ::AxisType sync_with = ::AxisType::XAxis,
        int index = -1) Q_DECL_OVERRIDE;

    virtual SciQLopPlotInterface* plot_impl(GetDataPyCallable callable,
        QString name = QStringLiteral("ColorMap"), ::DataOrder data_order = ::DataOrder::RowMajor,
        bool y_log_scale = false, bool z_log_scale = false,
        ::PlotType plot_type = ::PlotType::BasicXY, ::AxisType sync_with = ::AxisType::XAxis,
        int index = -1) Q_DECL_OVERRIDE;

public:
    SciQLopMultiPlotPanel(
        QWidget* parent = nullptr, bool synchronize_x = true, bool synchronize_time = false);

    void addPlot(SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    void removePlot(SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    SciQLopPlotInterface* plotAt(int index) const Q_DECL_OVERRIDE;
    const QList<SciQLopPlotInterface*>& plots() const;

    virtual void insertPlot(int index, SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    virtual void movePlot(int from, int to) Q_DECL_OVERRIDE;
    virtual void movePlot(SciQLopPlotInterface* plot, int to) Q_DECL_OVERRIDE;
    void clear() Q_DECL_OVERRIDE;


    bool contains(SciQLopPlotInterface* plot) const Q_DECL_OVERRIDE;

    bool empty() const Q_DECL_OVERRIDE;
    std::size_t size() const Q_DECL_OVERRIDE;

    void addWidget(QWidget* widget);
    void removeWidget(QWidget* widget);

    SciQLopPlot* createPlot(int index = -1);

    void set_x_axis_range(double lower, double upper) Q_DECL_OVERRIDE;

    void registerBehavior(SciQLopPlotCollectionBehavior* behavior) Q_DECL_OVERRIDE;
    void removeBehavior(const QString& type_name) Q_DECL_OVERRIDE;


#ifndef BINDINGS_H
    Q_SIGNAL void plotListChanged(const QList<SciQLopPlotInterface*>& plots);
#endif // BINDINGS_H
protected:
    void keyPressEvent(QKeyEvent* event) override;
};

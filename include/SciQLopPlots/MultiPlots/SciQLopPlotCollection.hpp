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

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "SciQLopPlots/enums.hpp"
#include <QColor>
#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>


class SciQLopPlotInterface;

class SciQLopPlotCollectionBehavior : public QObject
{
    Q_OBJECT
public:
    SciQLopPlotCollectionBehavior(QObject* parent = nullptr) : QObject(parent) { }
    inline virtual Q_SLOT void updatePlotList(const QList<SciQLopPlotInterface*>& plots) { }
};

class SciQLopPlotCollectionInterface
{

public:
    virtual void addPlot(SciQLopPlotInterface* plot) = 0;
    virtual void insertPlot(int index, SciQLopPlotInterface* plot) = 0;
    virtual void removePlot(SciQLopPlotInterface* plot) = 0;
    virtual void movePlot(int from, int to) = 0;
    virtual void movePlot(SciQLopPlotInterface* plot, int to) = 0;
    virtual void clear() = 0;

    virtual SciQLopPlotInterface* plotAt(int index) = 0;
    virtual const QList<SciQLopPlotInterface*>& plots() const = 0;

    virtual bool contains(SciQLopPlotInterface* plot) const = 0;

    virtual bool empty() const = 0;
    virtual std::size_t size() const = 0;

    virtual void set_x_axis_range(double min, double max) = 0;

    virtual void registerBehavior(SciQLopPlotCollectionBehavior* behavior) = 0;
    virtual void removeBehavior(const QString& type_name) = 0;

    inline virtual SciQLopPlotInterface* plot(Array_view x, Array_view y,
        QStringList labels = QStringList(), QList<QColor> colors = QList<QColor>(),
        ::DataOrder data_order = ::DataOrder::RowMajor, ::PlotType plot_type = ::PlotType::BasicXY,
        ::GraphType graph_type = ::GraphType::Line, int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual SciQLopPlotInterface* plot(Array_view x, Array_view y, Array_view z,
        const QString& name = QStringLiteral("ColorMap"),
        ::DataOrder data_order = ::DataOrder::RowMajor, bool y_log_scale = false,
        bool z_log_scale = false, ::PlotType plot_type = ::PlotType::BasicXY, int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual SciQLopPlotInterface* plot(GetDataPyCallable callable,
        QStringList labels = QStringList(), QList<QColor> colors = QList<QColor>(),
        ::DataOrder data_order = ::DataOrder::RowMajor, ::PlotType plot_type = ::PlotType::BasicXY,
        ::GraphType graph_type = ::GraphType::Line, int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }
};

template <typename U, typename Interface_T, typename... Args>
void registerBehavior(Interface_T* interface, Args&&... args)
{
    interface->registerBehavior(new U(interface, std::forward<Args>(args)...));
}

template <typename U, typename Interface_T>
void removeBehavior(Interface_T* interface)
{
    interface->removeBehavior(U::staticMetaObject.className());
}

class SciQLopPlotCollection : public QObject, public SciQLopPlotCollectionInterface
{
    Q_OBJECT
    QList<SciQLopPlotInterface*> _plots;

    QMap<QString, SciQLopPlotCollectionBehavior*> _behaviors;

public:
    SciQLopPlotCollection(QObject* parent = nullptr);
    virtual ~SciQLopPlotCollection() = default;
    void addPlot(SciQLopPlotInterface* plot) final;
    void insertPlot(int index, SciQLopPlotInterface* plot) final;
    void removePlot(SciQLopPlotInterface* plot) final;
    void movePlot(int from, int to) final;
    void movePlot(SciQLopPlotInterface* plot, int to) final;
    void clear() final;

    SciQLopPlotInterface* plotAt(int index) final;
    inline const QList<SciQLopPlotInterface*>& plots() const final { return _plots; }

    inline bool contains(SciQLopPlotInterface* plot) const final { return _plots.contains(plot); }

    inline bool empty() const final { return _plots.isEmpty(); }
    inline std::size_t size() const final { return _plots.size(); }

    virtual void set_x_axis_range(double lower, double upper) final;

    void registerBehavior(SciQLopPlotCollectionBehavior* behavior) final;
    void removeBehavior(const QString& type_name) final;

#ifndef BINDINGS_H
    Q_SIGNAL void plotListChanged(const QList<SciQLopPlotInterface*>& plots);
#endif // BINDINGS_H
};

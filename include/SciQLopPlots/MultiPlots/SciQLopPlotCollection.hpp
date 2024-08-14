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

#include "SciQLopPlots/SciQLopPlot.hpp"

class SciQLopPlotCollectionBehavior : public QObject
{
    Q_OBJECT
public:
    SciQLopPlotCollectionBehavior(QObject* parent = nullptr) : QObject(parent) { }
    inline virtual Q_SLOT void updatePlotList(const QList<SciQLopPlotInterface*>& plots) { }
};

class SciQLopPlotCollectionInterface
{

protected:
    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> plot_impl(const PyBuffer& x,
        const PyBuffer& y, QStringList labels = QStringList(),
        QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
        ::GraphType graph_type = ::GraphType::Line, int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> plot_impl(const PyBuffer& x,
        const PyBuffer& y, const PyBuffer& z, QString name = QStringLiteral("ColorMap"),
        bool y_log_scale = false, bool z_log_scale = false,
        ::PlotType plot_type = ::PlotType::BasicXY, int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> plot_impl(
        GetDataPyCallable callable, QStringList labels = QStringList(),
        QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
        ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr, int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> plot_impl(
        GetDataPyCallable callable, QString name = QStringLiteral("ColorMap"),
        bool y_log_scale = false, bool z_log_scale = false,
        ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr, int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }

public:
    virtual ~SciQLopPlotCollectionInterface() = default;

    inline virtual void addPlot(SciQLopPlotInterface* plot) { }
    inline virtual void insertPlot(int index, SciQLopPlotInterface* plot) { }
    inline virtual void removePlot(SciQLopPlotInterface* plot) { }
    inline virtual void movePlot(int from, int to) { }
    inline virtual void movePlot(SciQLopPlotInterface* plot, int to) { }
    inline virtual void clear() { }

    inline virtual SciQLopPlotInterface* plotAt(int index) const { return nullptr; }

    inline virtual bool contains(SciQLopPlotInterface* plot) const { return false; }

    inline virtual bool empty() const { return true; }
    inline virtual std::size_t size() const { return 0; }

    inline virtual void set_x_axis_range(double min, double max) { }

    inline virtual void registerBehavior(SciQLopPlotCollectionBehavior* behavior) { }
    inline virtual void removeBehavior(const QString& type_name) { }


    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> line(const PyBuffer& x,
        const PyBuffer& y, QStringList labels = QStringList(),
        QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
        int index = -1)
    {
        return plot_impl(x, y, labels, colors, plot_type, ::GraphType::Line, index);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> parametric_curve(
        const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
        QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
        int index = -1)
    {
        return plot_impl(x, y, labels, colors, plot_type, ::GraphType::ParametricCurve, index);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> colormap(const PyBuffer& x,
        const PyBuffer& y, const PyBuffer& z, QString name = QStringLiteral("ColorMap"),
        bool y_log_scale = false, bool z_log_scale = false,
        ::PlotType plot_type = ::PlotType::BasicXY, int index = -1)
    {
        return plot_impl(x, y, z, name, y_log_scale, z_log_scale, plot_type, index);
    }


    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> line(
        GetDataPyCallable callable, QStringList labels = QStringList(),
        QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
        QObject* sync_with = nullptr, int index = -1)
    {
        return plot_impl(callable, labels, colors, ::GraphType::Line, plot_type, sync_with, index);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> parametric_curve(
        GetDataPyCallable callable, QStringList labels = QStringList(),
        QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
        QObject* sync_with = nullptr, int index = -1)
    {
        return plot_impl(
            callable, labels, colors, ::GraphType::ParametricCurve, plot_type, sync_with, index);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> colormap(
        GetDataPyCallable callable, QString name = QStringLiteral("ColorMap"),
        bool y_log_scale = false, bool z_log_scale = false,
        ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr, int index = -1)
    {
        return plot_impl(callable, name, y_log_scale, z_log_scale, plot_type, sync_with, index);
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

    SciQLopPlotInterface* plotAt(int index) const final;
    inline const QList<SciQLopPlotInterface*>& plots() const { return _plots; }

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

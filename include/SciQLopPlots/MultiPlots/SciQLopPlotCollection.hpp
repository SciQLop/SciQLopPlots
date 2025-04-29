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

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"

Q_DECLARE_METATYPE(QList<SciQLopPlotInterface*>)

class SciQLopPlotPanelInterface;

class SciQLopPlotCollectionBehavior : public QObject
{
    Q_OBJECT

protected:
    QList<QPointer<SciQLopPlotInterface>> _plots;

    void _update_collection(auto & collection, const auto& items, auto&& connect, auto&& disconnect)
    {
        for (auto& item : items)
        {
            if (!item.isNull())
            {
                if (!collection.contains(item))
                {
                    connect(item); // #SciQLop-check-ignore-connect
                }
            }
        }
        for (auto& item : collection)
        {
            if (!item.isNull())
            {
                if (!items.contains(item))
                {
                    disconnect(item);
                }
            }
        }
        collection = items;
    }

public:
    SciQLopPlotCollectionBehavior(QObject* parent = nullptr) : QObject(parent) { }

    inline virtual Q_SLOT void updatePlotList(const QList<QPointer<SciQLopPlotInterface>>& plots)
    {
        WARN_ABSTRACT_METHOD;
    }

    inline virtual Q_SLOT void plotAdded(SciQLopPlotInterface* plot) { WARN_ABSTRACT_METHOD; }
    inline virtual Q_SLOT void plotRemoved(SciQLopPlotInterface* plot) { WARN_ABSTRACT_METHOD; }
    inline  virtual Q_SLOT  void panelAdded(SciQLopPlotPanelInterface* panel) {WARN_ABSTRACT_METHOD;}
    inline  virtual Q_SLOT  void panelRemoved(SciQLopPlotPanelInterface* panel) {WARN_ABSTRACT_METHOD;}

};

class SciQLopPlotCollectionInterface
{

protected:
    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    plot_impl(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
              ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, int index = -1,QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    plot_impl(const QList<PyBuffer>& values, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(),
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, int index = -1, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
    plot_impl(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
              QString name = QStringLiteral("ColorMap"), bool y_log_scale = false,
              bool z_log_scale = false, ::PlotType plot_type = ::PlotType::BasicXY, int index = -1, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    plot_impl(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr,
              int index = -1, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
    plot_impl(GetDataPyCallable callable, QString name = QStringLiteral("ColorMap"),
              bool y_log_scale = false, bool z_log_scale = false,
              ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr,
              int index = -1, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

public:
    virtual ~SciQLopPlotCollectionInterface() = default;

    inline virtual QList<QPointer<SciQLopPlotInterface>> plots() const
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual QList<QWidget*> child_widgets() const
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    virtual inline std::size_t plot_count() const { return plots().size(); }

    inline virtual void replot(bool immediate = false) { WARN_ABSTRACT_METHOD; }

    inline virtual void add_plot(SciQLopPlotInterface* plot) { WARN_ABSTRACT_METHOD; }
    inline virtual void insert_plot(int index, SciQLopPlotInterface* plot) { WARN_ABSTRACT_METHOD; }
    inline virtual void remove_plot(SciQLopPlotInterface* plot) { WARN_ABSTRACT_METHOD; }

    inline virtual void move_plot(int from, int to) { WARN_ABSTRACT_METHOD; }
    inline virtual void move_plot(SciQLopPlotInterface* plot, int to) { WARN_ABSTRACT_METHOD; }

    inline virtual void clear() { WARN_ABSTRACT_METHOD; }

    inline virtual int index(SciQLopPlotInterface* plot) const
    {
        WARN_ABSTRACT_METHOD;
        return -1;
    }

    inline virtual int index(const QPointF& pos) const
    {
        WARN_ABSTRACT_METHOD;
        return -1;
    }

    inline virtual int index_from_global_position(const QPointF& pos) const
    {
        WARN_ABSTRACT_METHOD;
        return -1;
    }

    inline virtual SciQLopPlotInterface* plot_at(int index) const
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual bool contains(SciQLopPlotInterface* plot) const
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    inline virtual bool empty() const
    {
        WARN_ABSTRACT_METHOD;
        return true;
    }

    inline virtual std::size_t size() const
    {
        WARN_ABSTRACT_METHOD;
        return 0;
    }

    inline virtual QList<QColor> color_palette() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual void set_color_palette(const QList<QColor>& palette) noexcept
    {
        WARN_ABSTRACT_METHOD;
    }

    virtual void set_x_axis_range(const SciQLopPlotRange& range);
    virtual const SciQLopPlotRange& x_axis_range() const;
    virtual void set_time_axis_range(const SciQLopPlotRange& range);
    virtual const SciQLopPlotRange& time_axis_range() const;

    inline virtual SciQLopPlotCollectionBehavior*
    register_behavior(SciQLopPlotCollectionBehavior* behavior)
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual SciQLopPlotCollectionBehavior* behavior(const QString& type_name) const
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual void remove_behavior(const QString& type_name) { WARN_ABSTRACT_METHOD; }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    line(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
         QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
         int index = -1, QVariantMap metaData={})
    {
        return plot_impl(x, y, labels, colors, plot_type, ::GraphType::Line,
                         ::GraphMarkerShape::NoMarker, index, metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    scatter(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
            QList<QColor> colors = QList<QColor>(),
            ::GraphMarkerShape marker = ::GraphMarkerShape::Cross,
            ::PlotType plot_type = ::PlotType::BasicXY, int index = -1, QVariantMap metaData={})
    {
        return plot_impl(x, y, labels, colors, plot_type, ::GraphType::Scatter, marker, index, metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    parametric_curve(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::PlotType plot_type = ::PlotType::BasicXY, int index = -1, QVariantMap metaData={})
    {
        return plot_impl(x, y, labels, colors, plot_type, ::GraphType::ParametricCurve,
                         ::GraphMarkerShape::NoMarker, index, metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
    colormap(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
             QString name = QStringLiteral("ColorMap"), bool y_log_scale = false,
             bool z_log_scale = false, ::PlotType plot_type = ::PlotType::BasicXY, int index = -1, QVariantMap metaData={})
    {
        return plot_impl(x, y, z, name, y_log_scale, z_log_scale, plot_type, index,metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    projection(const QList<PyBuffer>& values, QStringList labels = QStringList(),
               QList<QColor> colors = QList<QColor>(), int index = -1, QVariantMap metaData={})
    {
        return plot_impl(values, labels, colors, ::GraphMarkerShape::NoMarker, index,metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    line(GetDataPyCallable callable, QStringList labels = QStringList(),
         QList<QColor> colors = QList<QColor>(), ::PlotType plot_type = ::PlotType::BasicXY,
         QObject* sync_with = nullptr, int index = -1, QVariantMap metaData={})
    {
        return plot_impl(callable, labels, colors, ::GraphType::Line, ::GraphMarkerShape::NoMarker,
                         plot_type, sync_with, index,metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    scatter(GetDataPyCallable callable, QStringList labels = QStringList(),
            QList<QColor> colors = QList<QColor>(),
            ::GraphMarkerShape marker = ::GraphMarkerShape::Cross,
            ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr,
            int index = -1, QVariantMap metaData={})
    {
        return plot_impl(callable, labels, colors, ::GraphType::Scatter, marker, plot_type,
                         sync_with, index,metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    parametric_curve(GetDataPyCallable callable, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr,
                     int index = -1, QVariantMap metaData={})
    {
        return plot_impl(callable, labels, colors, ::GraphType::ParametricCurve,
                         ::GraphMarkerShape::NoMarker, plot_type, sync_with, index,metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
    colormap(GetDataPyCallable callable, QString name = QStringLiteral("ColorMap"),
             bool y_log_scale = false, bool z_log_scale = false,
             ::PlotType plot_type = ::PlotType::BasicXY, QObject* sync_with = nullptr,
             int index = -1, QVariantMap metaData={})
    {
        return plot_impl(callable, name, y_log_scale, z_log_scale, plot_type, sync_with, index,metaData);
    }

    inline virtual QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
    projection(GetDataPyCallable callable, QStringList labels = QStringList(),
               QList<QColor> colors = QList<QColor>(), QObject* sync_with = nullptr, int index = -1, QVariantMap metaData={})
    {
        return plot_impl(callable, labels, colors, ::GraphType::ParametricCurve,
                         ::GraphMarkerShape::NoMarker, ::PlotType::Projections, sync_with, index,metaData);
    }
};

template <typename U, typename Interface_T, typename... Args>
U* register_behavior(Interface_T* interface, Args&&... args)
{
    return qobject_cast<U*>(
        interface->register_behavior(new U(interface, std::forward<Args>(args)...)));
}

template <typename U, typename Interface_T>
U* behavior(Interface_T* interface)
{
    return qobject_cast<U*>(interface->behavior(U::staticMetaObject.className()));
}

template <typename U, typename Interface_T>
void remove_behavior(Interface_T* interface)
{
    interface->remove_behavior(U::staticMetaObject.className());
}

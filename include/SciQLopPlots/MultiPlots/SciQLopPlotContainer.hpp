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
#include <QSplitter>

#include "SciQLopPlotCollection.hpp"

class SciQLopPlotInterface;

class SciQLopPlotContainer : public QSplitter, public SciQLopPlotCollectionInterface
{
    Q_OBJECT

    QMap<QString, SciQLopPlotCollectionBehavior*> _behaviors;
    QList<QColor> _color_palette;
    SciQLopPlotRange _time_axis_range;
    SciQLopPlotRange _x_axis_range;

public:
    Q_PROPERTY(bool empty READ empty FINAL);

    SciQLopPlotContainer(QWidget* parent = nullptr);
    virtual ~SciQLopPlotContainer();

    void insertWidget(int index, QWidget* widget);
    void addWidget(QWidget* widget);
    void insert_plot(int index, SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    void add_plot(SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    void move_plot(int from, int to) Q_DECL_OVERRIDE;
    void move_plot(SciQLopPlotInterface* plot, int to) Q_DECL_OVERRIDE;
    void remove_plot(SciQLopPlotInterface* plot) Q_DECL_OVERRIDE;
    void remove_plot(SciQLopPlotInterface* plot, bool destroy);
    void removeWidget(QWidget* widget, bool destroy);

    inline int index(SciQLopPlotInterface* plot) const Q_DECL_OVERRIDE
    {
        return plots().indexOf(plot);
    }

    inline virtual int index(const QPointF& pos) const Q_DECL_OVERRIDE
    {
        const auto _plots = plots();
        for (decltype(_plots.size()) i = 0; i < _plots.size(); i++)
        {
            auto plot = _plots[i];
            if (plot->geometry().contains(pos.toPoint()))
                return i;
        }
        return -1;
    }

    virtual void clear() Q_DECL_OVERRIDE;

    inline virtual SciQLopPlotInterface* plot_at(int index) const Q_DECL_OVERRIDE
    {
        return plots().at(index);
    }

    virtual inline QList<QPointer<SciQLopPlotInterface>> plots() const Q_DECL_OVERRIDE
    {
        QList<QPointer<SciQLopPlotInterface>> plots;
        plots.reserve(count());
        for (int i = 0; i < count(); i++)
        {
            if (auto p = qobject_cast<SciQLopPlotInterface*>(widget(i)); p)
            {
                plots.emplace_back(p);
            }
        }
        return plots;
    }

    inline virtual bool contains(SciQLopPlotInterface* plot) const Q_DECL_OVERRIDE
    {
        return plots().contains(plot);
    }

    inline virtual bool empty() const Q_DECL_OVERRIDE { return count() == 0; }

    virtual std::size_t size() const Q_DECL_OVERRIDE { return count(); }

    inline void set_x_axis_range(const SciQLopPlotRange& range) Q_DECL_OVERRIDE
    {
        _x_axis_range = range;
        for (auto plot : plots())
        {
            plot->x_axis()->set_range(range);
        }
    }

    inline virtual const SciQLopPlotRange& x_axis_range() const Q_DECL_OVERRIDE
    {
        return _x_axis_range;
    }

    inline virtual void set_time_axis_range(const SciQLopPlotRange& range) Q_DECL_OVERRIDE
    {
        _time_axis_range = range;
        for (auto plot : plots())
        {
            plot->time_axis()->set_range(range);
        }
    }

    inline virtual const SciQLopPlotRange& time_axis_range() const Q_DECL_OVERRIDE
    {
        return _time_axis_range;
    }

    void register_behavior(SciQLopPlotCollectionBehavior* behavior) Q_DECL_OVERRIDE;

    void remove_behavior(const QString& type_name) Q_DECL_OVERRIDE;

    void organize_plots();

    inline virtual QList<QColor> color_palette() const noexcept override { return _color_palette; }

    inline virtual void set_color_palette(const QList<QColor>& palette) noexcept override
    {
        _color_palette = palette;
    }

#ifndef BINDINGS_H
    Q_SIGNAL void plot_list_changed(const QList<QPointer<SciQLopPlotInterface>>& plots);
#endif // BINDINGS_H
};

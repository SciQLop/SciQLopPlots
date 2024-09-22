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
    SciQLopPlotCollection* _plots;

public:
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
        return _plots->index(plot);
    }

    inline virtual int index(const QPointF& pos) const Q_DECL_OVERRIDE
    {
        for (auto i = 0UL; i < _plots->size(); i++)
        {
            auto plot = _plots->plot_at(i);
            if (plot->geometry().contains(pos.toPoint()))
                return i;
        }
        return -1;
    }

    virtual void clear() Q_DECL_OVERRIDE;

    inline virtual SciQLopPlotInterface* plot_at(int index) const Q_DECL_OVERRIDE
    {
        return _plots->plot_at(index);
    }

    virtual inline QList<SciQLopPlotInterface*> plots() const Q_DECL_OVERRIDE
    {
        return _plots->plots();
    }

    inline virtual bool contains(SciQLopPlotInterface* plot) const Q_DECL_OVERRIDE
    {
        return _plots->contains(plot);
    }

    inline virtual bool empty() const Q_DECL_OVERRIDE { return _plots->empty(); }

    virtual std::size_t size() const Q_DECL_OVERRIDE { return _plots->size(); }

    inline void set_x_axis_range(const SciQLopPlotRange& range) Q_DECL_OVERRIDE
    {
        _plots->set_x_axis_range(range);
    }

    inline virtual const SciQLopPlotRange& x_axis_range() const Q_DECL_OVERRIDE
    {
        return _plots->x_axis_range();
    }

    inline virtual void set_time_axis_range(const SciQLopPlotRange& range) Q_DECL_OVERRIDE
    {
        _plots->set_time_axis_range(range);
    }

    inline virtual const SciQLopPlotRange& time_axis_range() const Q_DECL_OVERRIDE
    {
        return _plots->time_axis_range();
    }

    inline void register_behavior(SciQLopPlotCollectionBehavior* behavior) Q_DECL_OVERRIDE
    {
        _plots->register_behavior(behavior);
    }

    inline void remove_behavior(const QString& type_name) Q_DECL_OVERRIDE
    {
        _plots->remove_behavior(type_name);
    }

    void organize_plots();

#ifndef BINDINGS_H
    Q_SIGNAL void plot_list_changed(const QList<SciQLopPlotInterface*>& plots);
#endif // BINDINGS_H
};

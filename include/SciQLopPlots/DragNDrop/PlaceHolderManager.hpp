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
#include "SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QLayout>
#include <QObject>
#include <QPointF>
#include <QWidget>


enum class DropLocation
{
    NewPlot,
    ExistingPlot,
};

struct DropResult
{
    DropLocation location;
    int index;
};

class PlaceHolder : public SciQLopPlotInterface
{
    Q_OBJECT

public:
    PlaceHolder(QWidget* parent = nullptr) : SciQLopPlotInterface(parent, "PlaceHolder")
    {
        setStyleSheet("background-color: #BBD5EE; border: 1px solid #2A7FD4");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
};

class PlaceHolderManager : public QObject
{
    Q_OBJECT

    enum class PlaceHolderLocation
    {
        None,
        Top,
        Bottom
    };

    PlaceHolder* _place_holder = nullptr;
    QWidget* _parent = nullptr;

    inline PlaceHolderLocation _compute_location(const QPointF& pos) const
    {
        if (auto index = _interface()->index(pos); index != -1)
        {
            auto plot = _interface()->plot_at(index);
            auto plot_top = plot->geometry().top();
            auto plot_bottom = plot->geometry().bottom();
            auto upper_zome = plot_top + 0.2 * plot->geometry().height();
            auto lower_zone = plot_bottom - 0.2 * plot->geometry().height();
            if (pos.y() < upper_zome)
            {
                return PlaceHolderLocation::Top;
            }
            if (pos.y() > lower_zone)
            {
                return PlaceHolderLocation::Bottom;
            }
        }
        return PlaceHolderLocation::None;
    }

    inline SciQLopPlotInterface* _plot_at(const QPointF& pos) const
    {
        if (auto index = _interface()->index(pos); index != -1)
        {
            return _interface()->plot_at(index);
        }
        return nullptr;
    }

    inline PlaceHolder* _create_place_holder(int index)
    {
        auto place_holder = new PlaceHolder();
        place_holder->setMinimumHeight(0.9 * _parent->height() / (1 + _interface()->size()));
        _interface()->insert_plot(index, place_holder);
        return place_holder;
    }

    inline void _remove_place_holder()
    {
        _interface()->remove_plot(_place_holder);
        _place_holder = nullptr;
    }

    inline PlaceHolder* _create_place_holder_if_needed(const QPointF& pos)
    {
        if (_interface()->empty())
        {
            return _create_place_holder(0);
        }
        auto location = _compute_location(pos);
        switch (location)
        {
            case PlaceHolderLocation::Top:
                return _create_place_holder(std::max(0, _interface()->index(pos)));
            case PlaceHolderLocation::Bottom:
                return _create_place_holder(_interface()->index(pos) + 1);
            default:
                break;
        }

        return nullptr;
    }

    inline SciQLopPlotCollectionInterface* _interface() const
    {
        return dynamic_cast<SciQLopPlotCollectionInterface*>(_parent);
    }


public:
    PlaceHolderManager(QWidget* parent = nullptr) : QObject(parent), _parent { parent } { }

    inline void dragEnterEvent(QDragEnterEvent* event)
    {
        _place_holder = _create_place_holder_if_needed(event->position());
    }

    inline void dragMoveEvent(QDragMoveEvent* event)
    {
        if (_place_holder)
        {
            if (_plot_at(event->position()) != _place_holder)
            {
                _remove_place_holder();
                return;
            }
        }
        else
        {
            _place_holder = _create_place_holder_if_needed(event->position());
        }
    }

    inline void dragLeaveEvent(QDragLeaveEvent* event)
    {
        if (_place_holder)
        {
            _remove_place_holder();
        }
    }

    inline DropResult dropEvent(QDropEvent* event)
    {
        if (_place_holder)
        {
            int index = _interface()->index(_place_holder);
            _remove_place_holder();
            return { DropLocation::NewPlot, index };
        }
        return { DropLocation::ExistingPlot,
            _interface()->index_from_global_position(QCursor::pos()) };
    }
};

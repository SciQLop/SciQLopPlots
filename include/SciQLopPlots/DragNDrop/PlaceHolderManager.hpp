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
#include "SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp"
#include <QFrame>
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

// A plain widget, not a plot: the panel must not sync its palette or time axis, nor list it.
class PlaceHolder : public QFrame
{
    Q_OBJECT

public:
    PlaceHolder(QWidget* parent = nullptr) : QFrame(parent)
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
    SciQLopPlotContainer* _container = nullptr;

    inline PlaceHolderLocation _compute_location(const QPointF& pos) const
    {
        if (auto widget = _interface()->widget_at(pos))
        {
            auto widget_top = widget->geometry().top();
            auto widget_bottom = widget->geometry().bottom();
            auto upper_zome = widget_top + 0.15 * widget->geometry().height();
            auto lower_zone = widget_bottom - 0.15 * widget->geometry().height();
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


    inline PlaceHolder* _create_place_holder(int index)
    {
        auto place_holder = new PlaceHolder();
        place_holder->setMinimumHeight(0.9 * _interface()->content_height() / (1 + _interface()->size()));
        _container->insertWidget(index, place_holder);
        return place_holder;
    }

    inline void _remove_place_holder()
    {
        _container->removeWidget(_place_holder, true);
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
    PlaceHolderManager(QWidget* parent, SciQLopPlotContainer* container)
            : QObject(parent), _parent { parent }, _container { container }
    {
    }

    inline void dragEnterEvent(QDragEnterEvent* event)
    {
        _place_holder = _create_place_holder_if_needed(event->position());
    }

    inline void dragMoveEvent(QDragMoveEvent* event)
    {
        if (_place_holder)
        {
            if (_interface()->widget_at(event->position()) != _place_holder)
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
            int index = _container->indexOf(_place_holder);
            _remove_place_holder();
            return { DropLocation::NewPlot, index };
        }
        return { DropLocation::ExistingPlot,
                 _interface()->index(event->position()) };
    }
};

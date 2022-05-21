/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
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
#include "../../enums.hpp"
#include "../../view.hpp"
#include <QCursor>
#include <memory>

namespace SciQLopPlots::interfaces
{
class IPlotWidget;

struct GraphicObject
{
    GraphicObject(IPlotWidget* plot, enums::Layers layer);
    virtual ~GraphicObject();

    virtual view::data_coordinates<2> center() const = 0;
    virtual view::pixel_coordinates<2> pix_center() const = 0;

    virtual void move(const view::data_coordinates<2>& delta) = 0;
    virtual void move(const view::pixel_coordinates<2>& delta) = 0;

    virtual bool contains(const view::data_coordinates<2>& position) const = 0;
    virtual bool contains(const view::pixel_coordinates<2>& position) const = 0;

    virtual void set_selected(bool select) = 0;

    inline virtual bool deletable() const { return m_deletable; }
    inline virtual void set_deletable(bool deletable) { m_deletable = deletable; }


    virtual Qt::CursorShape cursor_shape() const = 0;

    virtual void start_edit(const view::pixel_coordinates<2>& position) = 0;
    virtual void update_edit(const view::pixel_coordinates<2>& position) = 0;
    virtual void stop_edit(const view::pixel_coordinates<2>& position) = 0;

    enums::Layers layer() { return m_layer; }

protected:
    bool m_deletable = true;
    IPlotWidget* plot;
    enums::Layers m_layer;
};

class IGraphicObjectFactory
{
public:
    inline virtual ~IGraphicObjectFactory() {};
    virtual GraphicObject* create(
        IPlotWidget* plot, const view::pixel_coordinates<2>& start_position)
        = 0;
};

template <typename callable_t>
class GraphicObjectFactory : public IGraphicObjectFactory
{
    callable_t callable;

public:
    GraphicObjectFactory(callable_t&& callable)
            : callable { std::forward<callable_t>(callable) } { }

    inline ~GraphicObjectFactory() {};
    inline virtual GraphicObject* create(
        IPlotWidget* plot, const view::pixel_coordinates<2>& start_position) override
    {
        return callable(plot, start_position);
    }
};

template <typename callable_t>
GraphicObjectFactory(callable_t&& callable) -> GraphicObjectFactory<callable_t>;

template <typename callable_t>
std::shared_ptr<IGraphicObjectFactory> make_shared_GraphicObjectFactory(callable_t&& callable)
{
    return std::make_shared<GraphicObjectFactory<callable_t>>(std::forward<callable_t>(callable));
}

}

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
#include "GraphicObject.hpp"
#include <QPoint>
#include <algorithm>
#include <array>
#include <vector>


namespace SciQLopPlots::interfaces
{

template <std::size_t layer_count>
struct LayeredGraphicObjectCollection
{
private:
    std::array<std::vector<GraphicObject*>, layer_count> layers;

public:
    inline LayeredGraphicObjectCollection() { }
    inline ~LayeredGraphicObjectCollection() { }

    inline void registerGraphicObject(
        interfaces::GraphicObject* go, std::size_t layer = layer_count - 1)
    {
        layers[layer].push_back(go);
    }

    inline void removeGraphicObject(interfaces::GraphicObject* go)
    {
        for (auto& layer : layers)
        {
            if (std::size(layer))
            {
                if (auto it = std::find(std::begin(layer), std::end(layer), go);
                    it != std::end(layer))
                {
                    std::swap(*it, layer.back());
                    layer.pop_back();
                }
            }
        }
    }

    inline GraphicObject* graphicObjectAt(const view::pixel_coordinates<2>& position)
    {
        for (auto& layer : layers)
        {
            if (std::size(layer))
            {
                for (auto go : layer)
                {
                    if (go->contains(position))
                        return go;
                }
            }
        }
        return nullptr;
    }
    inline GraphicObject* nextGraphicObjectAt(const view::pixel_coordinates<2>& position, GraphicObject* current)
    {
        bool found_current = false;
        for (auto& layer : layers)
        {
            if (std::size(layer))
            {
                for (auto go : layer)
                {
                    if (go->contains(position) and found_current)
                    {
                        return go;
                    }
                    else if (go == current)
                    {
                        found_current = true;
                    }
                }
            }
        }
        return graphicObjectAt(position);
    }
};
}

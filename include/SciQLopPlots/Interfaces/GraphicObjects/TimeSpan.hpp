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

#include "../IPlotWidget.hpp"
#include "./ITimeSpan.hpp"

namespace SciQLopPlots::interfaces
{

template <typename TimeSpanImpl, typename plot_t>
class TimeSpan : public ITimeSpan
{
    TimeSpanImpl* time_span_impl;

public:
    TimeSpan(plot_t* plot, const axis::range time_range)
            : ITimeSpan { plot }
            , time_span_impl { new TimeSpanImpl { plot, time_range } }
    {
    }

    ~TimeSpan()
    {
        delete time_span_impl;
    }

    virtual void set_range(const axis::range& time_range) final
    {
        time_span_impl->set_range(time_range);
        emit range_changed(time_range);
    };
    virtual axis::range range() const final { return time_span_impl->range(); };

    virtual view::data_coordinates<2> center() const override { return time_span_impl->center(); }

    virtual view::pixel_coordinates<2> pix_center() const override
    {
        return time_span_impl->pix_center();
    }

    virtual void move(const view::data_coordinates<2>& delta) override
    {
        time_span_impl->move(delta);
    }

    virtual void move(const view::pixel_coordinates<2>& delta) override
    {
        time_span_impl->move(delta);
    }

    virtual bool contains(const view::data_coordinates<2>& position) const override
    {
        return time_span_impl->contains(position);
    }

    virtual bool contains(const view::pixel_coordinates<2>& position) const override
    {
        return time_span_impl->contains(position);
    }

    virtual void set_selected(bool select) override
    {
        return time_span_impl->set_selected(select);
    }
};

}

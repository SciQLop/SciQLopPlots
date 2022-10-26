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

#include <QObject>
#include <QScrollArea>
#include <QTimer>
#include <QWidget>

#include <QVBoxLayout>
#include <QList>


#include <cassert>

#include "plot.hpp"
#include "SciQLopPlots/Qt/QCustomPlot/QCPTimeSpan.hpp"
#include "SciQLopPlots/Interfaces/GraphicObjects/ITimeSpan.hpp"
#include <cpp_utils/containers/algorithms.hpp>

#include "SciQLopPlots/axis_range.hpp"

namespace SciQLopPlots
{
class TimeSpan : public interfaces::ITimeSpan
{
    SciQLopPlots::QCPWrappers::QCPTimeSpan* m_time_span;

public:
    TimeSpan(PlotWidget* plot, const axis::range time_range)
            : ITimeSpan { plot }, m_time_span { new SciQLopPlots::QCPWrappers::QCPTimeSpan { plot, time_range } }
    {
        connect(m_time_span, &SciQLopPlots::QCPWrappers::QCPTimeSpan::range_changed, this, &TimeSpan::range_changed);
    }

    ~TimeSpan() { delete m_time_span; }


    inline virtual void set_range(const axis::range& time_range) final
    {
        m_time_span->set_range(time_range);
    };

    inline virtual axis::range range() const final { return m_time_span->range(); };

    inline virtual view::data_coordinates<2> center() const override
    {
        return m_time_span->center();
    }

    inline virtual view::pixel_coordinates<2> pix_center() const override
    {
        return m_time_span->pix_center();
    }

    inline virtual void move(const view::data_coordinates<2>& delta) override
    {
        m_time_span->move(delta);
    }

    inline virtual void move(const view::pixel_coordinates<2>& delta) override
    {
        m_time_span->move(delta);
    }

    inline virtual bool contains(const view::data_coordinates<2>& position) const override
    {
        return m_time_span->contains(position);
    }

    inline virtual bool contains(const view::pixel_coordinates<2>& position) const override
    {
        return m_time_span->contains(position);
    }

    inline virtual void set_selected(bool select) override
    {
        return m_time_span->set_selected(select);
    }

    inline virtual Qt::CursorShape cursor_shape() const override
    {
        return m_time_span->cursor_shape();
    }

    inline virtual void start_edit(const view::pixel_coordinates<2>& position) override
    {
        m_time_span->start_edit(position);
    }
    inline virtual void update_edit(const view::pixel_coordinates<2>& position) override
    {
        m_time_span->update_edit(position);
    }
    inline virtual void stop_edit(const view::pixel_coordinates<2>& position) override
    {
        m_time_span->stop_edit(position);
    }
};
}

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

#include <QLayout>
#include <QObject>
#include <QWidget>

#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "SciQLopPlots/axis_range.hpp"

namespace SciQLopPlots::interfaces
{

class IGraph : public QObject
{
    Q_OBJECT

public:
    Q_SIGNAL void xRangeChanged(axis::range newRange);
    Q_SIGNAL void dataChanged();
    Q_SIGNAL void closed();

    IGraph(QObject* parent = nullptr) : QObject(parent) { }
};

class ILineGraph : public IGraph
{
    Q_OBJECT

public:
    ILineGraph(QObject* parent = nullptr) : IGraph(parent) { }
    virtual void plot(const std::vector<double>& x, const std::vector<double>& y);
};

class IMultiLineGraph : public IGraph
{
    Q_OBJECT

public:
    IMultiLineGraph(QObject* parent = nullptr) : IGraph(parent) { }
    virtual void plot(const std::vector<double>& x, const std::vector<double>& y);
};

class IColorMapGraph : public IGraph
{
    Q_OBJECT

public:
    IColorMapGraph(QObject* parent = nullptr) : IGraph(parent) { }
    virtual void plot(
        const std::vector<double>& x, const std::vector<double>& y, const std::vector<double> z);
};

}

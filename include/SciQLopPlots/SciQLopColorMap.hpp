/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#include "numpy_wrappers.hpp"

#include <QMutex>
#include <qcustomplot.h>

class SciQLopColorMap : public QObject
{
    NpArray_view _x;
    NpArray_view _y;
    NpArray_view _z;
    QCPRange _data_x_range;
    QCPAxis* _keyAxis;
    QCPAxis* _valueAxis;
    QCPColorMap* _cmap;
    QMutex _data_swap_mutex;
    Q_OBJECT
    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }


    void _range_changed(const QCPRange& newRange, const QCPRange& oldRange);
    void _resample(const QCPRange& newRange);
    void _setDataLinear();
    void _setDataLog();

public:
    enum class DataOrder
    {
        xFirst,
        yFirst
    };
    Q_ENUMS(FractionStyle)
    explicit SciQLopColorMap(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,const QString& name,
        DataOrder dataOrder = DataOrder::xFirst);
    virtual ~SciQLopColorMap() override;

    void setData(NpArray_view&& x, NpArray_view&& y, NpArray_view&& z);
    inline QCPColorMap* colorMap() const { return _cmap; }

    Q_SIGNAL void range_changed(const QCPRange& newRange, bool missData);

private:
    DataOrder _dataOrder = DataOrder::xFirst;
};

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

class SciQLopGraph : public QObject
{
    NpArray_view _x;
    NpArray_view _y;
    QCPRange _data_x_range;
    QCPAxis* _keyAxis;
    QCPAxis* _valueAxis;
    QList<QCPGraph*> _graphs;
    QMutex _data_swap_mutex;
    Q_OBJECT
    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    inline void _create_graphs(QStringList labels)
    {
        for (const auto& label : labels)
        {
            _graphs.append(_plot()->addGraph(_keyAxis, _valueAxis));
            _graphs.back()->setName(label);
            _graphs.back()->setAdaptiveSampling(true);
        }
    }


    void _range_changed(const QCPRange& newRange, const QCPRange& oldRange);
    void _resample(const QCPRange& newRange);

    void _setGraphData(std::size_t index, QVector<QCPGraphData> data);

    Q_SIGNAL void _setGraphDataSig(std::size_t index, QVector<QCPGraphData> data);

public:
    enum class DataOrder
    {
        xFirst,
        yFirst
    };
    Q_ENUMS(FractionStyle)
    explicit SciQLopGraph(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
        QStringList labels, DataOrder dataOrder = DataOrder::xFirst);
    virtual ~SciQLopGraph() override;

    void setData(NpArray_view&& x, NpArray_view&& y, bool ignoreCurrentRange = false);
    inline QCPGraph* graphAt(std::size_t index) const { return _graphs[index]; }

    Q_SIGNAL void range_changed(const QCPRange& newRange, bool missData);

private:
    DataOrder _dataOrder = DataOrder::xFirst;
};

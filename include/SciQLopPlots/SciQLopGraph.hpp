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
#include <qcustomplot.h>
struct GraphResampler;
class QThread;

class SciQLopGraph : public QObject
{
    GraphResampler* _resampler = nullptr;
    QThread* _resampler_thread = nullptr;

    QCPAxis* _keyAxis;
    QCPAxis* _valueAxis;
    QList<QCPGraph*> _graphs;

    Q_OBJECT

    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }


    void _range_changed(const QCPRange& newRange, const QCPRange& oldRange);

    void _setGraphData(std::size_t index, QVector<QCPGraphData> data);

    Q_SIGNAL void _setGraphDataSig(std::size_t index, QVector<QCPGraphData> data);

    void clear_graphs(bool graph_already_removed = false);
    void clear_resampler();
    void create_resampler(const QStringList &labels);
    void graph_got_removed_from_plot(QCPGraph* graph);

public:
    enum class DataOrder
    {
        xFirst,
        yFirst
    };
    Q_ENUMS(FractionStyle)
    explicit SciQLopGraph(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
        const QStringList& labels, DataOrder dataOrder = DataOrder::xFirst);

    explicit SciQLopGraph(QCustomPlot* parent, QCPAxis* keyAxis, QCPAxis* valueAxis,
        DataOrder dataOrder = DataOrder::xFirst);

    virtual ~SciQLopGraph() override;

    void setData(NpArray_view&& x, NpArray_view&& y, bool ignoreCurrentRange = false);
    inline QCPGraph* graphAt(std::size_t index) const { return _graphs[index]; }
    void create_graphs(const QStringList& labels);

    inline std::size_t line_count() { return std::size(this->_graphs); }

    Q_SIGNAL void range_changed(const QCPRange& newRange, bool missData);

private:
    DataOrder _dataOrder = DataOrder::xFirst;
};

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

#include "BufferProtocol.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include <qcustomplot.h>
struct GraphResampler;
class QThread;

class SciQLopGraph : public SQPQCPAbstractPlottableWrapper
{
    GraphResampler* _resampler = nullptr;
    QThread* _resampler_thread = nullptr;

    QCPAxis* _keyAxis;
    QCPAxis* _valueAxis;
    bool _auto_scale_y = false;

    Q_OBJECT

    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }


    void _range_changed(const QCPRange& newRange, const QCPRange& oldRange);

    void _setGraphData(std::size_t index, QVector<QCPGraphData> data);

#ifndef BINDINGS_H
    Q_SIGNAL void _setGraphDataSig(std::size_t index, QVector<QCPGraphData> data);
#endif // !BINDINGS

    void clear_graphs(bool graph_already_removed = false);
    void clear_resampler();
    void create_resampler(const QStringList& labels);

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

    void setData(Array_view&& x, Array_view&& y, bool ignoreCurrentRange = false);
    inline const QList<QCPGraph*> graphs() const noexcept
    {
        QList<QCPGraph*> graphs;
        for (auto plottable : m_plottables)
            graphs.append(qobject_cast<QCPGraph*>(plottable));
        return graphs;
    }
    inline QCPGraph* graphAt(std::size_t index) const
    {
        if (index < plottable_count())
            return qobject_cast<QCPGraph*>(m_plottables[index]);
        return nullptr;
    }
    void create_graphs(const QStringList& labels);

    inline std::size_t line_count() const noexcept { return plottable_count(); }

    void set_auto_scale_y(bool auto_scale_y);
    inline bool auto_scale_y() const noexcept { return _auto_scale_y; }

#ifndef BINDINGS_H
    Q_SIGNAL void auto_scale_y_changed(bool);
#endif

private:
    DataOrder _dataOrder = DataOrder::xFirst;
};

/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/DataProducer/DataProducer.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/enums.hpp"
#include <qcustomplot.h>
#include <QSignalBlocker>
#include <memory>
#include <optional>

//! A 1-D histogram of every column of y, with shared bin edges (QCPHistogram).
class SciQLopHistogram : public SciQLopGraphInterface
{
    Q_OBJECT
    QPointer<QCPHistogram> _hist;
    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    QList<SciQLopPyBuffer> _buffers;
    std::optional<HistogramStyle> _style;
    bool m_selected = false;
    bool _had_result = false;
    bool _edges_moved = false;
    QMetaObject::Connection _key_log_connection;

    QCustomPlot* _plot() const;
    QCPPlottableLegendItem* _legend_item() const;
    void _bin(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y);
    void _apply_default_style(std::size_t columns);
    void _on_edges_changed();
    void _on_histogram_ready();
    void _reset_edges_on_log_of_key_axis();

public:
    explicit SciQLopHistogram(QCustomPlot* parent, SciQLopPlotAxis* xAxis, SciQLopPlotAxis* yAxis,
                              const QString& name, int bins = 64, QVariantMap metadata = {});
    virtual ~SciQLopHistogram() override;

    //! Bins every column of y; x only has to match y's length.
    //! \throws std::invalid_argument if y is not 1-D or 2-D, or x and y lengths differ.
    Q_SLOT virtual void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y) override;
    virtual QList<SciQLopPyBuffer> data() const noexcept override { return _buffers; }
    //! \throws std::invalid_argument if values is not 1-D or 2-D.
    void set_values(SciQLopPyBuffer values);
    //! \throws std::invalid_argument unless edges has one more entry than heights has rows.
    void set_binned(SciQLopPyBuffer edges, SciQLopPyBuffer heights);

    //! \throws std::invalid_argument if bins <= 0.
    void set_bins(int bins);
    int bins() const noexcept;
    //! Not set_range: SciQLopFunctionGraph observers drive a set_range(SciQLopPlotRange) slot
    //! with the fetch window; the bin range is a different thing.
    //! \throws std::invalid_argument if a bound is not finite, lower >= upper, or lower <= 0
    //!         with log bins.
    void set_bin_range(double lower, double upper);
    void clear_bin_range();
    void reset_edges();
    QList<double> bin_edges() const;
    QList<QList<double>> bin_heights() const;

    void set_log_bins(bool log);
    bool log_bins() const noexcept;
    void set_normalization(HistogramNormalization normalization);
    HistogramNormalization normalization() const noexcept;
    void set_style(HistogramStyle style);
    HistogramStyle style() const noexcept;

    inline QCPHistogram* histogram() const { return _hist; }

    virtual QString layer() const noexcept override;
    virtual QStringList labels() const noexcept override { return { name() }; }
    virtual void set_colors(const QList<QColor>& colors) override;
    virtual QList<QColor> colors() const noexcept override;
    virtual QList<SciQLopGraphComponentInterface*> components() const noexcept override
    {
        return {};
    }

    virtual bool busy() const noexcept override;
    virtual void set_busy(bool busy) noexcept override;

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    //! The bin edges moved.
    Q_SIGNAL void bins_changed();

protected:
    void _named(const QString& name) override;
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override { return m_selected; }
    void _set_selected(bool selected) override;
};

class SciQLopHistogramFunction : public SciQLopHistogram, public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopHistogramFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                      SciQLopPlotAxis* yAxis, GetDataPyCallable&& callable,
                                      const QString& name, int bins = 64);
    virtual ~SciQLopHistogramFunction() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }
};

class SciQLopHistogramRemote : public SciQLopHistogram, public SciQLopRemoteGraph
{
    Q_OBJECT

public:
    explicit SciQLopHistogramRemote(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                    SciQLopPlotAxis* yAxis, const QString& name, int bins = 64,
                                    QVariantMap metadata = {});
    ~SciQLopHistogramRemote() override = default;

    inline void invalidate_cache() noexcept override { invalidate_pipeline_cache(); }

    inline bool busy() const noexcept override { return remote_busy(); }
    // Same as SciQLopHistogram2DRemote: the base forwards to QCPHistogram, whose busyChanged
    // already reaches busy_changed, so block it and emit exactly once from remote_busy().
    inline void set_busy(bool busy) noexcept override
    {
        set_remote_busy(busy);
        {
            const QSignalBlocker blocker(this);
            SciQLopHistogram::set_busy(busy);
        }
        Q_EMIT busy_changed(busy);
    }
};

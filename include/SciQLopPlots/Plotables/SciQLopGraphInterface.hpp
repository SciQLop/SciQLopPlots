/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"
#include "SciQLopPlots/SciQLopDrawnObject.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"

#include "SciQLopPlots/DataProducer/DataProducer.hpp"

#include <QColor>
#include <QObjectBindableProperty>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QWidget>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class SciQLopPlotAxisInterface;
class QCPColorScale;
class InspectorExtension;
class InspectorExtensionHolder;

class SciQLopPlottableInterface : public SciQLopDrawnObject
{
    Q_OBJECT

protected:
    SciQLopPlotRange m_range;
    SciQLopPlotRange m_data_range; // key range of currently loaded data
    QVariantMap m_metaData;
    std::unique_ptr<InspectorExtensionHolder> m_extension_holder;
    // The ctor assigns a unique placeholder objectName ("Line0", "Curve1", …)
    // used as an internal id. That id is NOT meaningful as a user-facing label,
    // so auto-naming of components must not derive from it — only from a name
    // the caller actually set via set_name().
    bool m_user_named = false;
    bool m_warned_dropped_colour = false;
    bool m_in_legend = true;

    //! Wires the legend items of qcp_plottables_of(this) to set_selected.
    void _connect_legend_selection();
    //! set_in_legend re-creates the legend items, and their connections with them.
    virtual void _legend_items_recreated() { _connect_legend_selection(); }

public:
    Q_PROPERTY(QList<SciQLopPyBuffer> data READ data NOTIFY data_changed BINDABLE bindable_data)
    Q_PROPERTY(bool busy READ busy WRITE set_busy NOTIFY busy_changed)

    SciQLopPlottableInterface(QVariantMap metaData={},QObject* parent = nullptr);

    virtual ~SciQLopPlottableInterface() = default;

    inline virtual QString layer() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QString();
    }

    virtual void set_range(const SciQLopPlotRange& range);

    // True once a caller has set a meaningful name via set_name() (i.e. the
    // objectName is no longer just the auto-generated placeholder id).
    inline bool has_user_name() const noexcept { return m_user_named; }

    virtual SciQLopPlotRange range() const noexcept { return m_range; }

    SciQLopPlotRange data_range() const noexcept { return m_data_range; }

    //! False keeps this plottable out of every legend, e.g. a decoration like a planet's outline.
    virtual void set_in_legend(bool in_legend);
    bool in_legend() const noexcept { return m_in_legend; }

    inline QVariantMap& meta_data(){return this->m_metaData;}
    inline const QVariantMap& meta_data() const {return this->m_metaData;}
    inline void set_meta_data(const QVariantMap& metaData) { this->m_metaData = metaData; }

    Q_SLOT virtual void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y) { WARN_ABSTRACT_METHOD; };

    Q_SLOT virtual void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z) { WARN_ABSTRACT_METHOD; };

    Q_SLOT virtual void set_data(const QList<SciQLopPyBuffer>& values) { WARN_ABSTRACT_METHOD; }

    /*!
     * \brief set_data_and_color A batch that carries its colour axis: \a data as set_data
     *        takes it, \a color one value per sample of data[0].
     *
     * Graphs that can draw a colour axis override this. The others keep the data, drop
     * the colour and warn once.
     */
    virtual void set_data_and_color(const QList<SciQLopPyBuffer>& data,
                                    const SciQLopPyBuffer& color);

    /*!
     * \brief set_color_data Map \a values onto this plottable's colour through \a gradient.
     *
     * One value per data point. Implemented by SciQLopLineGraph and SciQLopCurve (line
     * segments and markers). Anything else
     * throws rather than dropping the data on the floor — a silent no-op here
     * used to look exactly like a working call.
     */
    Q_SLOT virtual void set_color_data(SciQLopPyBuffer values, ::ColorGradient gradient = ColorGradient::Jet)
    {
        Q_UNUSED(values);
        Q_UNUSED(gradient);
        throw std::runtime_error(std::string(metaObject()->className())
                                 + " does not support per-point colour data");
    }

    virtual QList<SciQLopPyBuffer> data() const noexcept
    {
        return m_data;
    }

    virtual QBindable<QList<SciQLopPyBuffer>> bindable_data() const noexcept
    {
        return QBindable<QList<SciQLopPyBuffer>>(&m_data);
    }

    virtual bool busy() const noexcept { return false; }
    virtual void set_busy(bool busy) noexcept { Q_UNUSED(busy); }

    inline virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
    {
        WARN_ABSTRACT_METHOD;
    }

    inline virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
    {
        WARN_ABSTRACT_METHOD;
    }

    inline virtual void set_z_axis(SciQLopPlotAxisInterface* axis) noexcept
    {
        WARN_ABSTRACT_METHOD;
    }

    // No axis is an answer, not an unsupported call: callers check for None.
    inline virtual SciQLopPlotAxisInterface* x_axis() const noexcept { return nullptr; }

    inline virtual SciQLopPlotAxisInterface* y_axis() const noexcept { return nullptr; }

    inline virtual SciQLopPlotAxisInterface* z_axis() const noexcept { return nullptr; }

    inline virtual std::size_t parent_plot_height() const noexcept
    {
        if (auto* w = qobject_cast<QWidget*>(parent()))
            return w->height();
        return 0;
    }

    inline virtual std::size_t parent_plot_width() const noexcept
    {
        if (auto* w = qobject_cast<QWidget*>(parent()))
            return w->width();
        return 0;
    }

    inline virtual QSize parent_plot_size() const noexcept
    {
        if (auto* w = qobject_cast<QWidget*>(parent()))
            return w->size();
        return {};
    }

    void add_inspector_extension(InspectorExtension* extension);
    void remove_inspector_extension(InspectorExtension* extension);
    QList<InspectorExtension*> inspector_extensions() const;

    virtual void invalidate_cache() noexcept { }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange range);
    Q_SIGNAL void inspector_extensions_changed();
    Q_SIGNAL void replot();
    Q_SIGNAL void data_changed();
    Q_SIGNAL void data_changed(SciQLopPyBuffer x, SciQLopPyBuffer y);
    Q_SIGNAL void data_changed(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z);
    Q_SIGNAL void data_changed(const QList<SciQLopPyBuffer>& values);
    Q_SIGNAL void busy_changed(bool busy);
    //! A data callback raised or returned data the graph can't take: the traceback or the reason.
    Q_SIGNAL void fetch_failed(const QString& message);
    Q_SIGNAL void parent_plot_resized(const QSize& size);
    Q_SIGNAL void request_rescale();

    protected:
    bool _got_first_data = false;

    //! A name given through set_name() is the caller's: it then labels late components.
    void _named(const QString& name) override
    {
        Q_UNUSED(name);
        m_user_named = true;
    }

    void check_first_data(std::size_t n);
    bool data_off_view() const;

private:
    friend class SciQLopPlot;
    //! The plot ran the first-data fit this graph asked for. When it fitted nothing (a pane
    //! that only fits with the others, after a caller's move), or the caller moved the batch
    //! off view so nothing of it got fitted, the next batch in view asks again. So a caller who
    //! pans away leaves the one-shot armed: the first pan back into the data re-fits.
    void _first_fit_ran(bool fitted)
    {
        if (!fitted || data_off_view())
            _got_first_data = false;
    }

protected:

    Q_OBJECT_BINDABLE_PROPERTY(SciQLopPlottableInterface, QList<SciQLopPyBuffer>, m_data, QOverload<const QList<SciQLopPyBuffer>&>::of(&SciQLopPlottableInterface::data_changed))
};

#ifndef BINDINGS_H
class QCPAbstractPlottable;
//! The NeoQCP plottables drawing \a plottable: one per component, or a colour map's own.
QList<QCPAbstractPlottable*> qcp_plottables_of(const SciQLopPlottableInterface* plottable);
#endif

class SciQLopGraphInterface : public SciQLopPlottableInterface
{
    Q_OBJECT

public:
    SciQLopGraphInterface(const QString& prefix = "Graph",QVariantMap metaData={}, QObject* parent = nullptr);
    virtual ~SciQLopGraphInterface() Q_DECL_OVERRIDE = default;

    virtual void set_labels(const QStringList& labels) { WARN_ABSTRACT_METHOD; }

    virtual void set_colors(const QList<QColor>& colors) { WARN_ABSTRACT_METHOD; }

    virtual QStringList labels() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QStringList();
    }

    virtual QList<QColor> colors() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QList<QColor>();
    }

    virtual SciQLopGraphComponentInterface* component(const QString& name) const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    virtual SciQLopGraphComponentInterface* component(int index) const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    virtual QList<SciQLopGraphComponentInterface*> components() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QList<SciQLopGraphComponentInterface*>();
    }

protected:
    void _legend_items_recreated() override;

public:
#ifndef BINDINGS_H
    // Appends finite y-values whose x falls in visible_key_range to `out`.
    // Multi-component graphs append every component. Default no-op — concrete
    // graphs override. Used by value-axis percentile autoscale (it then runs
    // nth_element over the pooled values).
    virtual void collect_visible_values(const SciQLopPlotRange& visible_key_range,
                                        std::vector<double>& out) const noexcept
    {
        Q_UNUSED(visible_key_range);
        Q_UNUSED(out);
    }

    //! Colour by a scalar, as the plot's shared colour scale sees it
    //! (see ColorScaleController). Graphs that cannot be coloured keep these defaults.
    virtual bool has_color_values() const { return false; }
    //! Min/max of the finite colour values; only the positive ones when \a log.
    virtual std::optional<std::pair<double, double>> color_range(bool log) const
    {
        Q_UNUSED(log);
        return std::nullopt;
    }
    //! Follow \a scale; nullptr: back to the graph's own range and gradient.
    virtual void attach_color_scale(QCPColorScale* scale) { Q_UNUSED(scale); }

    Q_SIGNAL void labels_changed(const QStringList& labels);
    Q_SIGNAL void colors_changed(const QList<QColor>& colors);
    Q_SIGNAL void component_list_changed();
#endif

protected:
    // A graph has one colour per component.
    QColor _color() const override
    {
        throw sqp::unsupported_call(unsupported_message("color", typeid(*this).name())
                                    + "; use colors() / set_colors()");
    }
};

class SciQLopColorMapInterface : public SciQLopPlottableInterface
{
    Q_OBJECT

public:
    SciQLopColorMapInterface(QVariantMap metaData={}, QObject* parent = nullptr);
    virtual ~SciQLopColorMapInterface() = default;

    inline virtual ColorGradient gradient() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return ColorGradient::Jet;
    }

    inline virtual void set_gradient(ColorGradient gradient) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_y_log_scale(bool y_log_scale) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_z_log_scale(bool z_log_scale) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual bool y_log_scale() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    inline virtual bool z_log_scale() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

protected:
    // A colour map's colours come from its gradient.
    QColor _color() const override
    {
        throw sqp::unsupported_call(unsupported_message("color", typeid(*this).name())
                                    + "; use gradient() / set_gradient()");
    }
};

inline void drop_batch(SciQLopPlottableInterface* g, const QString& why)
{
    qWarning() << "SciQLopPlots: dropping data batch from provider for" << g->objectName() << ":"
               << why;
    Q_EMIT g->fetch_failed(why);
}

// Runs `apply` (a batch reaching the graph), guarding against exceptions escaping
// a queued connection (which would std::terminate): a rejected batch is dropped.
template <typename Apply>
inline void apply_or_drop_batch(SciQLopPlottableInterface* g, Apply&& apply)
{
    try
    {
        apply();
    }
    catch (const std::exception& e)
    {
        drop_batch(g, QString::fromUtf8(e.what()));
    }
}

inline QString wrong_arity_message(int expected, qsizetype got)
{
    return QStringLiteral("expected %1, got %2 arrays")
        .arg(expected == 2 ? QStringLiteral("(x, y)") : QStringLiteral("(x, y, z)"))
        .arg(got);
}

// Wire a pipeline's new_data_* signals to graph->set_data, dropping a batch that
// throws. N picks the arity: 2 -> set_data(x, y), 3 -> set_data(x, y, z), else
// set_data(list) for every batch size (the graph checks it). A 2 or 3 graph reports
// a batch of any other size instead of losing it.
// new_data_colored always goes to graph->set_data_and_color.
template <typename Pipeline>
inline QList<QMetaObject::Connection>
connect_pipeline_data_to_graph(Pipeline* pipeline, SciQLopPlottableInterface* graph, int N)
{
    const auto apply = [](SciQLopPlottableInterface* g, auto&&... buffers)
    { apply_or_drop_batch(g, [&] { g->set_data(std::forward<decltype(buffers)>(buffers)...); }); };
    const auto as_list = [N, apply](SciQLopPlottableInterface* g, QList<SciQLopPyBuffer> values)
    {
        if (N == 2 || N == 3)
            drop_batch(g, wrong_arity_message(N, values.size()));
        else
            apply(g, values);
    };
    const auto pair = [N, apply, as_list](SciQLopPlottableInterface* g, SciQLopPyBuffer x,
                                          SciQLopPyBuffer y)
    {
        if (N == 2)
            apply(g, std::move(x), std::move(y));
        else
            as_list(g, { std::move(x), std::move(y) });
    };
    const auto triple = [N, apply, as_list](SciQLopPlottableInterface* g, SciQLopPyBuffer x,
                                            SciQLopPyBuffer y, SciQLopPyBuffer z)
    {
        if (N == 3)
            apply(g, std::move(x), std::move(y), std::move(z));
        else
            as_list(g, { std::move(x), std::move(y), std::move(z) });
    };
    QList<QMetaObject::Connection> conns;
    conns << QObject::connect(pipeline, &Pipeline::new_data_2d, graph,
                              [g = graph, pair](SciQLopPyBuffer x, SciQLopPyBuffer y)
                              { pair(g, std::move(x), std::move(y)); });
    conns << QObject::connect(
        pipeline, &Pipeline::new_data_3d, graph,
        [g = graph, triple](SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z)
        { triple(g, std::move(x), std::move(y), std::move(z)); });
    conns << QObject::connect(pipeline, &Pipeline::new_data_nd, graph,
                              [g = graph, as_list](const QList<SciQLopPyBuffer>& values)
                              { as_list(g, values); });
    conns << QObject::connect(pipeline, &Pipeline::fetch_failed, graph,
                              &SciQLopPlottableInterface::fetch_failed);
    conns << QObject::connect(pipeline, &Pipeline::new_data_colored, graph,
        [g = graph](const QList<SciQLopPyBuffer>& data, const SciQLopPyBuffer& color)
        { apply_or_drop_batch(g, [&] { g->set_data_and_color(data, color); }); });
    return conns;
}

class SciQLopFunctionGraph
{
protected:
    SimplePyCallablePipeline* m_pipeline;

    QList<QMetaObject::Connection> m_observer_connections;

    template <typename... Args>
    void connect(Args&&... args) // #SciQLop-check-ignore-connect
    {
        m_observer_connections.append(
            QObject::connect(std::forward<Args>(args)...)); // #SciQLop-check-ignore-connect
    }

    SciQLopPlottableInterface* as_graph = nullptr;
    QMetaObject::Connection m_idle_connection;

public:
    SciQLopFunctionGraph() { }

    SciQLopFunctionGraph(GetDataPyCallable&& callable, SciQLopPlottableInterface* as_graph,
                         int N = 2);

    virtual ~SciQLopFunctionGraph() = default;

    inline virtual void set_callable(GetDataPyCallable callable)
    {
        m_pipeline->set_callable(std::move(callable));
    }

    inline virtual GetDataPyCallable callable() const noexcept { return m_pipeline->callable(); }

    virtual void observe(QObject* observable);

    inline virtual void observe(QObject* observable, const char* signal) { }

    inline virtual void observe(QObject* observable, const QString& signal)
    {
        observe(observable, signal.toStdString().c_str());
    }

    inline virtual void call(const SciQLopPlotRange& range) noexcept
    {
        as_graph->set_busy(true);
        m_pipeline->call(range);
    }

    inline virtual void call(SciQLopPyBuffer x, SciQLopPyBuffer y) noexcept
    {
        as_graph->set_busy(true);
        m_pipeline->call(x, y);
    }

    inline virtual void call(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z) noexcept
    {
        as_graph->set_busy(true);
        m_pipeline->call(x, y, z);
    }

    inline virtual void call(const QList<SciQLopPyBuffer>& values) noexcept
    {
        as_graph->set_busy(true);
        m_pipeline->call(values);
    }

    inline void invalidate_pipeline_cache() noexcept { m_pipeline->invalidate_cache(); }

    //! See DataProviderInterface::set_prefetch_margin.
    inline void set_prefetch_margin(double margin) noexcept { m_pipeline->set_prefetch_margin(margin); }
    inline double prefetch_margin() const noexcept { return m_pipeline->prefetch_margin(); }
    //! See DataProviderInterface::set_prefetch_budget_bytes.
    inline void set_prefetch_budget_bytes(double bytes) noexcept { m_pipeline->set_prefetch_budget_bytes(bytes); }
    inline double prefetch_budget_bytes() const noexcept { return m_pipeline->prefetch_budget_bytes(); }
};

// Mixin that binds a RemoteDataPipeline to a graph. Sibling of SciQLopFunctionGraph.
// Busy semantics differ from the callable model: pipeline_idle fires as soon as
// the request is emitted (the remote get_data returns immediately), so we must NOT
// clear busy on pipeline_idle. Instead: busy=true on range request out, busy=false
// when data actually arrives.
//
// The mixin owns the busy flag because QCP components don't exist until the first
// set_data call — any component-based busy() would silently return false before
// that. Concrete classes delegate busy()/set_busy() here.
class SciQLopRemoteGraph
{
protected:
    RemoteDataPipeline* m_pipeline = nullptr;
    SciQLopPlottableInterface* as_graph = nullptr;
    QList<QMetaObject::Connection> m_connections;
    bool m_busy = false;

public:
    SciQLopRemoteGraph() { }
    SciQLopRemoteGraph(SciQLopPlottableInterface* as_graph, int N = 2);
    virtual ~SciQLopRemoteGraph() = default;

    // The bound channel endpoint: SciQLop connects channel.data_requested to its
    // IPC send and calls channel.set_data(...) with shared-memory-backed numpy.
    inline RemoteDataPipeline* remote_channel() const noexcept { return m_pipeline; }

    inline void invalidate_pipeline_cache() noexcept { m_pipeline->invalidate_cache(); }

    //! See DataProviderInterface::set_prefetch_margin.
    inline void set_prefetch_margin(double margin) noexcept { m_pipeline->set_prefetch_margin(margin); }
    inline double prefetch_margin() const noexcept { return m_pipeline->prefetch_margin(); }
    //! See DataProviderInterface::set_prefetch_budget_bytes.
    inline void set_prefetch_budget_bytes(double bytes) noexcept { m_pipeline->set_prefetch_budget_bytes(bytes); }
    inline double prefetch_budget_bytes() const noexcept { return m_pipeline->prefetch_budget_bytes(); }

    inline bool remote_busy() const noexcept { return m_busy; }
    inline void set_remote_busy(bool busy) noexcept { m_busy = busy; }
};

//! Gives a freshly made graph the caller's name. An empty name keeps the automatic one.
inline SciQLopGraphInterface* with_name(SciQLopGraphInterface* graph, const QString& name)
{
    if (graph && !name.isEmpty())
        graph->set_name(name);
    return graph;
}

//! Same for a panel factory's (plot, graph) pair.
template <typename Plot>
QPair<Plot*, SciQLopGraphInterface*> with_name(QPair<Plot*, SciQLopGraphInterface*> pair,
                                               const QString& name)
{
    with_name(pair.second, name);
    return pair;
}

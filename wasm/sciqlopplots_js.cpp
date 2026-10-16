// SciQLopPlots for JavaScript: the embind layer under sciqlopplots.mjs. Every panel, plot,
// plottable and span gets an integer id; sciqlopplots.mjs wraps the ids in objects.
// A panel fills one Qt screen, i.e. one container element the page handed to Qt
// (qtAddContainerElement, then createPanel). JavaScript owns the data: it pushes arrays and
// refetches when a panel's time range changes.
//
// The flat calls without a panel id (addPlot(), setTimeRange(start, stop), ...) are the
// first API, kept for existing pages: they act on a panel filling the first container.
#include "SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp"
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/SciQLopTheme.hpp"

#include <QApplication>
#include <QBackingStore>
#include <QOpenGLContext>
#include <QFile>
#include <QPalette>
#include <QPointer>
#include <QRhiWidget>
#include <QScreen>
#include <qpa/qplatformbackingstore.h>
#include <rhi/qrhi.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <magic_enum/magic_enum.hpp>

#include <cmath>
#include <cstdio>
#include <map>
#include <numeric>

using emscripten::val;

namespace
{

std::vector<QPointer<QObject>> objects;
int default_panel_id = -1;

// Buffer-protocol format code and item size of each typed array kind; the only dtypes
// dispatch_dtype handles on wasm.
const std::map<std::string, std::pair<char, std::size_t>> typed_array_formats {
    { "Float64Array", { 'd', 8 } },
    { "Float32Array", { 'f', 4 } },
};

// A projection's axis modes, by their JavaScript name.
const std::map<std::string, ProjectionAxisLinking> axis_linkings {
    { "independent", ProjectionAxisLinking::Independent },
    { "linked", ProjectionAxisLinking::ByDimension },
    { "sameScale", ProjectionAxisLinking::SameScale },
};

// Theme colours a page can set, by their JavaScript name.
const std::map<std::string, void (SciQLopTheme::*)(const QColor&)> theme_colors {
    { "background", &SciQLopTheme::set_background },
    { "foreground", &SciQLopTheme::set_foreground },
    { "grid", &SciQLopTheme::set_grid },
    { "subGrid", &SciQLopTheme::set_sub_grid },
    { "selection", &SciQLopTheme::set_selection },
    { "legendBackground", &SciQLopTheme::set_legend_background },
    { "legendBorder", &SciQLopTheme::set_legend_border },
};

SciQLopPyBuffer buffer_from_array(val array, val shape)
{
    const auto kind = array["constructor"]["name"].as<std::string>();
    const auto format = typed_array_formats.find(kind);
    if (format == typed_array_formats.end())
        throw std::invalid_argument("Buffer.fromArray: unsupported array type " + kind);
    // The buffer reads as many items as its shape says: a shape larger than the array would
    // read past it.
    const auto dims = emscripten::vecFromJSArray<std::size_t>(shape);
    const auto items = std::accumulate(dims.begin(), dims.end(), std::size_t { 1 },
                                       std::multiplies<> {});
    if (dims.empty() || items != array["length"].as<std::size_t>())
        throw std::invalid_argument("Buffer.fromArray: shape does not match the array's length");
    const auto bytes = val::global("Uint8Array")
                           .new_(array["buffer"], array["byteOffset"], array["byteLength"]);
    return SciQLopPyBuffer(emscripten::convertJSArrayToNumberVector<unsigned char>(bytes),
                           format->second.first, format->second.second, dims);
}

// One id per object, so a handle the page gets again (plotAt, pane) matches the one it has.
// simplify: a linear scan, fine for a page's few hundred objects; a QHash when pages hold more.
int keep(QObject* object)
{
    const auto known = std::find(objects.cbegin(), objects.cend(), object);
    if (known != objects.cend())
        return int(known - objects.cbegin());
    objects.emplace_back(object);
    return int(objects.size()) - 1;
}

template <typename T>
T* get(int id)
{
    auto* object = (id >= 0 && id < int(objects.size())) ? objects[id].data() : nullptr;
    if (auto* typed = dynamic_cast<T*>(object))
        return typed;
    throw std::out_of_range("SciQLopPlots: no such object (or wrong kind): " + std::to_string(id));
}

template <typename T>
T* owner(QObject* object)
{
    for (auto* parent = object->parent(); parent; parent = parent->parent())
        if (auto* typed = dynamic_cast<T*>(parent))
            return typed;
    return nullptr;
}

QString qstring(val v)
{
    return QString::fromStdString(v.as<std::string>());
}

// A CSS colour (name, #rgb, #rrggbbaa, rgb(), rgba(), hsl(), ...), parsed by the browser:
// QColor reads neither rgb()/rgba() nor #rrggbbaa (it takes #aarrggbb), and an unparsed colour
// draws opaque black.
QColor css_color(const std::string& css)
{
    static val context = val::global("document")
                             .call<val>("createElement", val("canvas"))
                             .call<val>("getContext", val("2d"));
    // fillStyle ignores what it cannot parse: two different sentinels tell an invalid colour.
    const auto normalized = [&](const char* sentinel)
    {
        context.set("fillStyle", sentinel);
        context.set("fillStyle", css);
        return context["fillStyle"].as<std::string>();
    };
    const auto color = normalized("#000000");
    if (color != normalized("#ffffff"))
        throw std::invalid_argument("SciQLopPlots: not a CSS colour: " + css);
    // The browser gives "#rrggbb" for opaque colours, "rgba(r, g, b, a)" otherwise.
    int r = 0, g = 0, b = 0;
    double a = 1.;
    if (std::sscanf(color.c_str(), "rgba(%d, %d, %d, %lf)", &r, &g, &b, &a) == 4)
        return QColor(r, g, b, int(std::lround(a * 255)));
    return QColor(QString::fromStdString(color));
}

QStringList string_list(val strings)
{
    QStringList list;
    for (const auto& s : emscripten::vecFromJSArray<std::string>(strings))
        list.append(QString::fromStdString(s));
    return list;
}

QList<QColor> color_list(val colors)
{
    QList<QColor> list;
    for (const auto& s : emscripten::vecFromJSArray<std::string>(colors))
        list.append(css_color(s));
    return list;
}

// options[name] when the page set it, else fallback.
val option(val options, const char* name, val fallback = val::undefined())
{
    return (options.isUndefined() || options.isNull() || !options.hasOwnProperty(name))
        ? fallback
        : options[name];
}

void set_theme(int panel_id, std::string base, val colors);

// Qt for wasm binds each WebGL context to its window's canvas, and its QRhi falls back to an
// offscreen surface when it needs a context outside a frame while another window's context is
// current: Qt then reports the context lost, rebuilds the QRhi, and plots of that window are left
// with a dangling one (a crash on the next repaint or removal). With one panel its context is
// always current; with several, make a window's own context current before a plot paints
// (QRhiWidget::paintEvent begins an offscreen frame) and before a removal frees GPU resources.
// simplify: works around QWasmOpenGLContext::makeCurrent() refusing offscreen surfaces;
// drop it once Qt for wasm makes the owning context current there.
void make_context_current(QWidget* panel)
{
    auto* store = panel->backingStore();
    auto* window = panel->windowHandle();
    if (!store || !store->handle() || !window)
        return;
    // Not QRhi::makeThreadLocalNativeContextCurrent(): outside a frame it takes the same
    // offscreen fallback. The context's own surface is the window it was created for.
    auto* rhi = store->handle()->rhi(window);
    if (!rhi || rhi->backend() != QRhi::OpenGLES2)
        return;
    if (auto* context = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles())->context;
        context && QOpenGLContext::currentContext() != context)
        context->makeCurrent(window);
}

struct OwnContextFirst : QObject
{
    using QObject::QObject;
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Paint)
            if (auto* plot = qobject_cast<QRhiWidget*>(watched))
                make_context_current(plot->window());
        return false;
    }
};

SciQLopMultiPlotPanel* make_panel(QScreen* screen)
{
    auto* panel = new SciQLopMultiPlotPanel(nullptr, false, true);
    // Qt for wasm wraps a framed window in an HTML frame with a light grey background, which
    // shows through the gaps between plots.
    panel->setWindowFlag(Qt::FramelessWindowHint);
    panel->setAutoFillBackground(true);
    panel->setFrameShape(QFrame::NoFrame);   // the page draws its own border
    panel->setScreen(screen);
    panel->setWindowState(Qt::WindowFullScreen);
    panel->show();
    return panel;
}


// screen_name: the id of the .qt-screen element Qt made in the panel's container.
int create_panel(std::string screen_name)
{
    const auto screens = QGuiApplication::screens();
    const auto screen = std::find_if(screens.begin(), screens.end(), [&](QScreen* s)
                                     { return s->name().toStdString() == screen_name; });
    if (screen == screens.end())
        throw std::invalid_argument("SciQLopPlots: no screen " + screen_name);
    const int id = keep(make_panel(*screen));
    set_theme(id, "light", val::undefined());   // else an empty panel shows Qt's grey
    return id;
}

SciQLopMultiPlotPanel* default_panel()
{
    if (default_panel_id < 0)
        default_panel_id = keep(make_panel(QGuiApplication::primaryScreen()));
    return get<SciQLopMultiPlotPanel>(default_panel_id);
}

int add_plot(int panel)
{
    return keep(get<SciQLopMultiPlotPanel>(panel)->create_plot(-1, PlotType::TimeSeries));
}

// A row of a panel that a page element covers (sciqlopplots.mjs lays the element over it), for
// another plotting library: it draws nothing, follows the panel's time range like a plot, and
// tells the page where it is, in CSS pixels from the panel's top-left corner.
class ElementRow : public SciQLopPlotInterface
{
    SciQLopPlotDummyAxis* m_time_axis = new SciQLopPlotDummyAxis(this);
    QList<QColor> m_palette;
    val m_on_geometry;
    // simplify: the chain is the one of the last reparenting; a row moved under a widget that
    // is itself reparented later would stop following. Rows only move within their panel.
    QList<QPointer<QWidget>> m_ancestors;

    static bool moves(QEvent* event)
    {
        const auto type = event->type();
        return type == QEvent::Move || type == QEvent::Resize || type == QEvent::Show
            || type == QEvent::Hide;
    }

    // { x, y, width, height }, or null while the row is not shown (removed rows are hidden).
    void report_geometry()
    {
        if (!isVisible())
            return (void)m_on_geometry(val::null());
        const auto corner = mapTo(window(), QPoint(0, 0));
        auto rect = val::object();
        rect.set("x", corner.x());
        rect.set("y", corner.y());
        rect.set("width", width());
        rect.set("height", height());
        m_on_geometry(rect);
    }

    // A row moves when any widget between it and the panel does: a splitter, a scroll.
    void watch_ancestors()
    {
        for (const auto& ancestor : std::as_const(m_ancestors))
            if (ancestor)
                ancestor->removeEventFilter(this);
        m_ancestors.clear();
        for (auto* ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget())
        {
            ancestor->installEventFilter(this);
            m_ancestors.append(ancestor);
        }
    }

public:
    // on_geometry(rect | null), on_time_range(start, stop); undefined for none.
    ElementRow(val on_geometry, val on_time_range) : m_on_geometry(std::move(on_geometry))
    {
        if (!on_time_range.isUndefined())
            connect(m_time_axis, &SciQLopPlotAxisInterface::range_changed, this,
                    [on_time_range](SciQLopPlotRange r) { on_time_range(r.start(), r.stop()); });
    }

    // 'removed' tells the page to take its element away: a panel's clear() deletes rows too.
    ~ElementRow() override { m_on_geometry(val("removed")); }

    // The panel shares its height out by size hints: as much room as a plot.
    QSize sizeHint() const override { return QSize(50, 50); }

    SciQLopPlotAxisInterface* time_axis() const noexcept override { return m_time_axis; }
    SciQLopPlotAxisInterface* x_axis() const noexcept override { return m_time_axis; }
    QList<QColor> color_palette() const noexcept override { return m_palette; }
    void set_color_palette(const QList<QColor>& colors) noexcept override { m_palette = colors; }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ParentChange)
            watch_ancestors();
        if (moves(event))
            report_geometry();
        return SciQLopPlotInterface::event(event);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (moves(event))
            report_geometry();
        return SciQLopPlotInterface::eventFilter(watched, event);
    }
};

int add_element_row(int panel, val on_geometry, val on_time_range)
{
    auto* row = new ElementRow(std::move(on_geometry), std::move(on_time_range));
    const int id = keep(row);
    get<SciQLopMultiPlotPanel>(panel)->add_plot(row);
    return id;
}

std::string kind_of(SciQLopPlotInterface* plot)
{
    if (qobject_cast<SciQLopNDProjectionPlot*>(plot))
        return "projection";
    return dynamic_cast<ElementRow*>(plot) ? "element" : "plot";
}

// The plot under (x, y), in CSS pixels from the panel's top-left corner (the panel fills its
// element): { id, index, kind: 'plot' | 'projection' | 'element' }, or null between and outside
// plots.
val plot_at(int panel_id, double x, double y)
{
    auto* panel = get<SciQLopMultiPlotPanel>(panel_id);
    for (auto* w = panel->childAt(QPoint(int(x), int(y))); w && w != panel; w = w->parentWidget())
    {
        auto* plot = qobject_cast<SciQLopPlotInterface*>(w);
        if (plot && panel->contains(plot))
        {
            auto hit = val::object();
            hit.set("id", keep(plot));
            hit.set("index", panel->index(plot));
            hit.set("kind", kind_of(plot));
            return hit;
        }
    }
    return val::null();
}

// Hands the page what was dropped on a panel: callback(target, data), target = { id, index,
// newPlot, kind }. As on the desktop, the panel draws a placeholder while the drag is near a
// plot's edge and makes the new plot itself before calling back.
class JsDropCallback : public PlotDragNDropCallback
{
    SciQLopMultiPlotPanel* m_panel;
    val m_callback;
    QList<QPointer<SciQLopPlotInterface>> m_plots_before_drop;

public:
    JsDropCallback(SciQLopMultiPlotPanel* panel, const QString& mime_type, val callback)
            : PlotDragNDropCallback(mime_type, true), m_panel(panel), m_callback(std::move(callback))
    {
        // A drop reaches the panel's viewport, not the panel: watch for it application-wide.
        qApp->installEventFilter(this);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Drop)
            if (auto* widget = qobject_cast<QWidget*>(watched); widget && m_panel->isAncestorOf(widget))
                m_plots_before_drop = m_panel->plots();
        return false;
    }

    void call(SciQLopPlotInterface* plot, const QMimeData* mime_data) override
    {
        auto target = val::object();
        target.set("id", keep(plot));
        target.set("index", m_panel->index(plot));
        target.set("newPlot", !m_plots_before_drop.contains(plot));
        target.set("kind", kind_of(plot));
        m_callback(target, QString::fromUtf8(mime_data->data(mime_type())).toStdString());
    }
};

// Qt 6.11's browser drag (QWasmDrag::onNativeDragOver) gives a drag's page position where the
// window's own is expected: for a window not at the page's corner, Qt looked for the drop target
// elsewhere and the drag was refused. This puts the position back in the window's frame.
class DragPositionFix : public QObject
{
    bool m_resending = false;

    static QPointF window_point(QWindow* window, QPointF page)
    {
        // Qt's global frame: viewport position minus the body's (QWasmScreen geometry).
        const auto view = val::global("window");
        const auto body = val::global("document")["body"].call<val>("getBoundingClientRect");
        return window->mapFromGlobal(page
                                     - QPointF(view["scrollX"].as<double>(), view["scrollY"].as<double>())
                                     - QPointF(body["left"].as<double>(), body["top"].as<double>()));
    }

public:
    using QObject::QObject;

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (m_resending || (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove))
            return false;
        auto* drag = static_cast<QDragMoveEvent*>(event);
        const auto at = window_point(static_cast<QWindow*>(watched), drag->position()).toPoint();
        std::unique_ptr<QDragMoveEvent> fixed;
        if (event->type() == QEvent::DragEnter)
            fixed = std::make_unique<QDragEnterEvent>(at, drag->possibleActions(), drag->mimeData(),
                                                      drag->buttons(), drag->modifiers());
        else
            fixed = std::make_unique<QDragMoveEvent>(at, drag->possibleActions(), drag->mimeData(),
                                                     drag->buttons(), drag->modifiers());
        m_resending = true;
        QCoreApplication::sendEvent(watched, fixed.get());
        m_resending = false;
        // Qt answers the browser from the event it sent.
        drag->setDropAction(fixed->dropAction());
        drag->setAccepted(fixed->isAccepted());
        return true;
    }
};

void accept_drops(int panel_id, std::string mime_type, val callback)
{
    auto* panel = get<SciQLopMultiPlotPanel>(panel_id);
    if (auto* window = panel->windowHandle(); !window->property("dragPositionFixed").toBool())
    {
        window->installEventFilter(new DragPositionFix(panel));
        window->setProperty("dragPositionFixed", true);
    }
    panel->add_accepted_mime_type(
        new JsDropCallback(panel, QString::fromStdString(mime_type), std::move(callback)));
}

// Removes any object by id, through its owner where it has one.
void remove_object(int id)
{
    auto* object = get<QObject>(id);
    if (auto* widget = dynamic_cast<QWidget*>(object); widget || owner<QWidget>(object))
        make_context_current((widget ? widget : owner<QWidget>(object))->window());
    if (auto* plottable = dynamic_cast<SciQLopPlottableInterface*>(object))
    {
        if (auto* plot = owner<SciQLopPlotInterface>(plottable))
            return plot->remove_plottable(plottable);
    }
    else if (auto* plot = dynamic_cast<SciQLopPlotInterface*>(object))
    {
        if (auto* panel = owner<SciQLopPlotPanelInterface>(plot))
            return panel->remove_plot(plot);
    }
    else if (auto* panel = dynamic_cast<SciQLopMultiPlotPanel*>(object))
    {
        // Plots it removed or cleared are deleted later, and their GPU widgets free resources
        // of the panel's QRhi: delete them while it still exists.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        // Now, not deleteLater(): the page removes the panel's screen right after this call.
        return delete panel;
    }
    object->deleteLater();
}

// A step longer than threshold x its neighbours breaks the line; 0 never breaks it.
void set_gap_threshold(QObject* object, double threshold)
{
    if (!std::isfinite(threshold) || threshold < 0)
        throw std::invalid_argument("SciQLopPlots: a gap threshold is a number >= 0, got "
                                    + std::to_string(threshold));
    if (auto* graph = qobject_cast<SciQLopMultiGraphBase*>(object))
        return graph->set_gap_threshold(threshold);
    throw std::invalid_argument("SciQLopPlots: only lines and scatters have a gap threshold");
}

void set_gap_threshold_of(int id, double threshold)
{
    set_gap_threshold(get<QObject>(id), threshold);
}

double gap_threshold(int id)
{
    auto* object = get<QObject>(id);
    if (auto* graph = qobject_cast<SciQLopMultiGraphBase*>(object))
        return graph->gap_threshold();
    throw std::invalid_argument("SciQLopPlots: only lines and scatters have a gap threshold");
}

// options: labels (string[]), colors (CSS colour strings), name, gapThreshold. The first API
// passed labels alone.
void style_graph(SciQLopGraphInterface* graph, val options)
{
    if (val::global("Array").call<bool>("isArray", options))
        return graph->set_labels(string_list(options));
    if (auto labels = option(options, "labels"); !labels.isUndefined())
        graph->set_labels(string_list(labels));
    if (auto colors = option(options, "colors"); !colors.isUndefined())
        graph->set_colors(color_list(colors));
    if (auto name = option(options, "name"); !name.isUndefined())
        graph->set_name(qstring(name));
    if (auto threshold = option(options, "gapThreshold"); !threshold.isUndefined())
        set_gap_threshold(graph, threshold.as<double>());
}

int line(int plot, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, val options)
{
    auto* graph = get<SciQLopPlotInterface>(plot)->line(x, y);
    style_graph(graph, options);
    return keep(graph);
}

int scatter(int plot, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, val options)
{
    auto* graph = get<SciQLopPlotInterface>(plot)->scatter(x, y);
    style_graph(graph, options);
    return keep(graph);
}

// An x/y curve drawn in data order (x need not grow): a trajectory, a model outline.
int curve(int plot, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, val options)
{
    auto* graph = get<SciQLopPlotInterface>(plot)->parametric_curve(x, y);
    style_graph(graph, options);
    return keep(graph);
}

// A [min, max] pair, checked here: indexing a missing one throws in JavaScript, past C++'s catch.
SciQLopPlotRange range_of(val ranges, int dim)
{
    const bool is_pair = val::global("Array").call<bool>("isArray", ranges)
        && dim < ranges["length"].as<int>() && val::global("Array").call<bool>("isArray", ranges[dim])
        && ranges[dim]["length"].as<int>() == 2;
    if (!is_pair)
        throw std::invalid_argument("SciQLopPlots: ranges needs a [min, max] for dimension "
                                    + std::to_string(dim));
    return SciQLopPlotRange(ranges[dim][0].as<double>(), ranges[dim][1].as<double>());
}

void hold_ranges(SciQLopNDProjectionPlot* projection, val ranges)
{
    const auto pairs = projection->dimension_pairs();
    for (int i = 0; i < projection->subplot_count(); ++i)
    {
        auto* pane = projection->subplot(i);
        pane->set_fit_first_data(false);
        const auto range = [&](int dim) { return range_of(ranges, dim); };
        pane->x_axis()->set_range(range(pairs[i].first));
        pane->y_axis()->set_range(range(pairs[i].second));
    }
}

void set_legend_visible(int plot, bool visible)
{
    if (auto* legend = get<SciQLopPlotInterface>(plot)->legend())
        legend->set_visible(visible);
}

// mode: one of axis_linkings' names.
void set_projection_axes(int projection, std::string mode)
{
    const auto linking = axis_linkings.find(mode);
    if (linking == axis_linkings.end())
        throw std::invalid_argument("SciQLopPlots: axes must be 'independent', 'linked' or "
                                    "'sameScale', not '" + mode + "'");
    get<SciQLopNDProjectionPlot>(projection)->set_axis_linking(linking->second);
}

// A row of 2-D projections of a 3-D trajectory, added to the panel's stack.
// options: pairs ([[x, y], ...] dimension indices, one per pane; default [[0,1],[1,2],[2,0]]),
// labels (the three dimension names), equalAspect (default true), ranges (a fixed [min, max] per
// dimension instead of fitting the first data; same spans keep equal aspect in data units),
// axes (a set_projection_axes mode, default "linked").
void configure_projection(SciQLopNDProjectionPlot* projection, val options)
{
    if (auto pairs = option(options, "pairs"); !pairs.isUndefined())
    {
        QList<QPair<int, int>> list;
        for (int i = 0; i < pairs["length"].as<int>(); ++i)
            list.append({ pairs[i][0].as<int>(), pairs[i][1].as<int>() });
        projection->set_dimension_pairs(list);
    }
    if (auto labels = option(options, "labels"); !labels.isUndefined())
        projection->set_axis_labels(string_list(labels));
    projection->set_equal_aspect_ratio(option(options, "equalAspect", val(true)).as<bool>());
    if (auto ranges = option(options, "ranges"); !ranges.isUndefined())
        hold_ranges(projection, ranges);
    if (auto axes = option(options, "axes"); !axes.isUndefined())
        set_projection_axes(keep(projection), axes.as<std::string>());
}

int add_projection(int panel_id, val options)
{
    auto* panel = get<SciQLopMultiPlotPanel>(panel_id);
    auto* projection
        = dynamic_cast<SciQLopNDProjectionPlot*>(panel->create_plot(-1, PlotType::Projections));
    // Bad options throw once the row is in the panel, and the page gets no handle to remove it.
    try
    {
        configure_projection(projection, options);
    }
    catch (...)
    {
        panel->remove_plot(projection);
        throw;
    }
    return keep(projection);
}

int projection_pane(int projection, int index)
{
    auto* pane = get<SciQLopNDProjectionPlot>(projection)->subplot(index);
    if (!pane)
        throw std::out_of_range("SciQLopPlots: no pane " + std::to_string(index));
    return keep(pane);
}

// t: times, then one buffer per dimension. options: name, timeColors (default true: the curve
// fades along time). set_time_marker follows it.
int trajectory(int projection, const SciQLopPyBuffer& t, const SciQLopPyBuffer& x,
               const SciQLopPyBuffer& y, const SciQLopPyBuffer& z, val options)
{
    auto* plot = get<SciQLopNDProjectionPlot>(projection);
    auto* graph = plot->parametric_curve(QList<SciQLopPyBuffer> { t, x, y, z });
    // Plot-wide, and it only reaches curves that exist: so after this one is made.
    plot->set_time_color_enabled(option(options, "timeColors", val(true)).as<bool>());
    if (auto name = option(options, "name"); !name.isUndefined())
        graph->set_name(qstring(name));
    return keep(graph);
}

// A static 3-D curve (a model), projected on every pane like the trajectory.
int reference_curve(int projection, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                    const SciQLopPyBuffer& z, val options)
{
    const auto color = option(options, "color");
    return keep(get<SciQLopNDProjectionPlot>(projection)->add_reference_curve(
        { x, y, z }, qstring(option(options, "name", val(""))),
        color.isUndefined() ? QColor() : css_color(color.as<std::string>())));
}

// NaN clears the markers.
void set_time_marker(int projection, double t)
{
    get<SciQLopNDProjectionPlot>(projection)->set_time_marker(t);
}

// options: name, yLog, zLog, gradient (a ColorGradient name, e.g. "Jet", "Viridis").
int colormap_with(int plot, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                  const SciQLopPyBuffer& z, val options)
{
    auto* map = get<SciQLopPlotInterface>(plot)->colormap(
        x, y, z, qstring(option(options, "name", val("ColorMap"))),
        option(options, "yLog", val(false)).as<bool>(),
        option(options, "zLog", val(false)).as<bool>());
    if (auto name = option(options, "gradient"); !name.isUndefined())
    {
        auto gradient = magic_enum::enum_cast<ColorGradient>(name.as<std::string>());
        if (!gradient)
            throw std::invalid_argument("unknown gradient " + name.as<std::string>());
        map->set_gradient(*gradient);
    }
    return keep(map);
}

int colormap(int plot, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
             const SciQLopPyBuffer& z, std::string name, bool y_log, bool z_log)
{
    auto options = val::object();
    options.set("name", name);
    options.set("yLog", y_log);
    options.set("zLog", z_log);
    return colormap_with(plot, x, y, z, options);
}

void set_data_xy(int id, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y)
{
    get<SciQLopPlottableInterface>(id)->set_data(x, y);
}

void set_data_xyz(int id, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                  const SciQLopPyBuffer& z)
{
    get<SciQLopPlottableInterface>(id)->set_data(x, y, z);
}

// A trajectory's data: the same four arrays as when it was made.
void set_data_txyz(int id, const SciQLopPyBuffer& t, const SciQLopPyBuffer& x,
                   const SciQLopPyBuffer& y, const SciQLopPyBuffer& z)
{
    get<SciQLopNDProjectionCurves>(id)->set_data(QList<SciQLopPyBuffer> { t, x, y, z });
}

void set_visible(int id, bool visible)
{
    get<SciQLopPlottableInterface>(id)->set_visible(visible);
}

SciQLopPlotAxisInterface* axis(int plot, const std::string& which)
{
    auto* p = get<SciQLopPlotInterface>(plot);
    if (which == "x")
        return p->x_axis();
    if (which == "y")
        return p->y_axis();
    if (which == "z")
        return p->z_axis();
    throw std::invalid_argument("axis is 'x', 'y' or 'z', not " + which);
}

// options: label, log, range ([min, max]).
void set_axis(int plot, std::string which, val options)
{
    auto* ax = axis(plot, which);
    if (auto label = option(options, "label"); !label.isUndefined())
        ax->set_label(qstring(label));
    if (auto log = option(options, "log"); !log.isUndefined())
        ax->set_log(log.as<bool>());
    if (auto range = option(options, "range"); !range.isUndefined())
        ax->set_range(SciQLopPlotRange(range[0].as<double>(), range[1].as<double>()));
}

val axis_range(int plot, std::string which)
{
    const auto range = axis(plot, which)->range();
    auto result = val::array();
    result.call<void>("push", range.start(), range.stop());
    return result;
}

void set_time_range(int panel, double start, double stop)
{
    get<SciQLopMultiPlotPanel>(panel)->set_time_axis_range(start, stop);
}

val time_range(int panel)
{
    const auto range = get<SciQLopMultiPlotPanel>(panel)->time_axis_range();
    auto result = val::array();
    result.call<void>("push", range.start(), range.stop());
    return result;
}

// base: "light" or "dark"; colors: any of theme_colors' names, as CSS colour strings.
// The theme colours the plots; the palette, the panel's widgets around them.
void set_theme(int panel_id, std::string base, val colors)
{
    auto* panel = get<SciQLopMultiPlotPanel>(panel_id);
    auto* theme = base == "dark" ? SciQLopTheme::dark(panel) : SciQLopTheme::light(panel);
    for (const auto& [name, setter] : theme_colors)
        if (auto color = option(colors, name.c_str()); !color.isUndefined())
            (theme->*setter)(css_color(color.as<std::string>()));
    if (auto* previous = panel->theme())
        previous->deleteLater();
    panel->set_theme(theme);
    QPalette palette(theme->background());   // derives the shades splitters and frames use
    palette.setColor(QPalette::Base, theme->background());
    for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        palette.setColor(role, theme->foreground());
    panel->setPalette(palette);
}

// A pinch over a panel, in the panel's coordinates: zooms the time axis of the plot under it by
// factor (> 1 zooms out), keeping the time under x_from under x_to, as SciQLop's pinch does.
void pinch_time(int panel_id, double x_from, double x_to, double y, double factor)
{
    auto* panel = get<SciQLopMultiPlotPanel>(panel_id);
    QWidget* widget = panel->childAt(QPoint(int(x_to), int(y)));
    while (widget && !qobject_cast<QCustomPlot*>(widget))
        widget = widget->parentWidget();
    auto* plot = qobject_cast<QCustomPlot*>(widget);
    if (!plot || !(factor > 0) || !std::isfinite(factor))
        return;
    const auto from = plot->mapFrom(panel, QPointF(x_from, y)).x();
    const auto to = plot->mapFrom(panel, QPointF(x_to, y)).x();
    auto* axis = plot->xAxis;
    const double anchor = axis->pixelToCoord(from);
    axis->scaleRange(factor, anchor);
    axis->moveRange(anchor - axis->pixelToCoord(to));
    plot->replot(QCustomPlot::rpQueuedReplot);
}

void clear(int panel)
{
    get<SciQLopMultiPlotPanel>(panel)->clear();
}

// options: color (CSS), readOnly, tooltip.
int add_span(int panel, double start, double stop, val options)
{
    return keep(new MultiPlotsVerticalSpan(
        get<SciQLopMultiPlotPanel>(panel), SciQLopPlotRange(start, stop, true),
        css_color(option(options, "color", val("rgba(100, 100, 220, 0.25)")).as<std::string>()),
        option(options, "readOnly", val(false)).as<bool>(), true,
        qstring(option(options, "tooltip", val("")))));
}

void set_span_range(int span, double start, double stop)
{
    get<MultiPlotsVerticalSpan>(span)->set_range(SciQLopPlotRange(start, stop, true));
}

val span_range(int span)
{
    const auto range = get<MultiPlotsVerticalSpan>(span)->range();
    auto result = val::array();
    result.call<void>("push", range.start(), range.stop());
    return result;
}

// Events: a panel's "timeRangeChanged" (start, stop); a plot's "cursorTimeChanged" (time, NaN
// when the mouse leaves); a span's "rangeChanged" (start, stop), "selectionChanged" (selected)
// and "deleteRequested" ().
void on(int id, std::string event, val callback)
{
    auto* object = get<QObject>(id);
    auto range_callback = [callback](SciQLopPlotRange r) { callback(r.start(), r.stop()); };
    if (auto* panel = dynamic_cast<SciQLopMultiPlotPanel*>(object); panel
        && event == "timeRangeChanged")
        QObject::connect(panel, &SciQLopPlotPanelInterface::time_range_changed, panel,
                         range_callback);
    else if (auto* span = dynamic_cast<MultiPlotsVerticalSpan*>(object); span
             && event == "rangeChanged")
        QObject::connect(span, &MultiPlotsVerticalSpan::range_changed, span, range_callback);
    else if (span && event == "selectionChanged")
        QObject::connect(span, &MultiPlotsVerticalSpan::selection_changed, span,
                         [callback](bool selected) { callback(selected); });
    else if (auto* plot = dynamic_cast<SciQLopPlotInterface*>(object); plot
             && event == "cursorTimeChanged")
        QObject::connect(plot, &SciQLopPlotInterface::cursor_time_changed, plot,
                         [callback](double t) { callback(t); });
    else if (span && event == "deleteRequested")
        QObject::connect(span, &MultiPlotsVerticalSpan::delete_requested, span,
                         [callback]() { callback(); });
    else
        throw std::invalid_argument("SciQLopPlots: no event " + event + " on object "
                                    + std::to_string(id));
}

// format: "png", "jpg" or "pdf"; width/height 0 keep the panel's size. The bytes of the file.
val export_panel(int panel, std::string format, int width, int height)
{
    const auto path = QStringLiteral("/tmp/sciqlopplots-export.") + QString::fromStdString(format);
    auto* p = get<SciQLopMultiPlotPanel>(panel);
    const bool saved = format == "pdf" ? p->save_pdf(path, width, height)
                                       : p->save(path, width, height);
    QFile file(path);
    if (!saved || !file.open(QIODevice::ReadOnly))
        throw std::runtime_error("SciQLopPlots: could not export the panel as " + format);
    const auto bytes = file.readAll();
    file.close();
    QFile::remove(path);
    return val(emscripten::typed_memory_view(bytes.size(),
                                             reinterpret_cast<const unsigned char*>(bytes.data())))
        .call<val>("slice");
}

std::string version()
{
    return SCIQLOPPLOTS_VERSION;
}

} // namespace

EMSCRIPTEN_BINDINGS(sciqlopplots)
{
    using emscripten::function;
    emscripten::class_<SciQLopPyBuffer>("Buffer")
        .class_function("fromArray", &buffer_from_array)
        .function("size", &SciQLopPyBuffer::flat_size);
    function("version", &version);
    function("createPanel", &create_panel);
    function("remove", &remove_object);
    function("addPlot", &add_plot);
    function("addElementRow", &add_element_row);
    function("plotAt", &plot_at);
    function("acceptDrops", &accept_drops);
    function("line", &line);
    function("scatter", &scatter);
    function("curve", &curve);
    function("setLegendVisible", &set_legend_visible);
    function("addProjection", &add_projection);
    function("projectionPane", &projection_pane);
    function("setProjectionAxes", &set_projection_axes);
    function("trajectory", &trajectory);
    function("referenceCurve", &reference_curve);
    function("setTimeMarker", &set_time_marker);
    function("colormap", &colormap_with);
    function("colormap", &colormap);
    function("setData", &set_data_xy);
    function("setData", &set_data_xyz);
    function("setData", &set_data_txyz);
    function("setVisible", &set_visible);
    function("setGapThreshold", &set_gap_threshold_of);
    function("gapThreshold", &gap_threshold);
    function("setAxis", &set_axis);
    function("axisRange", &axis_range);
    function("setTimeRange", &set_time_range);
    function("timeRange", &time_range);
    function("setTheme", &set_theme);
    function("clear", &clear);
    function("pinchTime", &pinch_time);
    function("addSpan", &add_span);
    function("setSpanRange", &set_span_range);
    function("spanRange", &span_range);
    function("on", &on);
    function("exportPanel", &export_panel);

    // The first API: one panel filling the first container, created on first use.
    function("addPlot", +[]() { default_panel(); return add_plot(default_panel_id); });
    function("setLineData", &set_data_xy);
    function("setColormapData", &set_data_xyz);
    function("setYLog", +[](int plot, bool log) { axis(plot, "y")->set_log(log); });
    function("setYLabel", +[](int plot, std::string label)
             { axis(plot, "y")->set_label(QString::fromStdString(label)); });
    function("setTheme", +[](std::string base, val colors)
             { default_panel(); set_theme(default_panel_id, base, colors); });
    function("setTimeRange", +[](double start, double stop)
             { default_panel(); set_time_range(default_panel_id, start, stop); });
    function("onTimeRangeChanged", +[](val callback)
             { default_panel(); on(default_panel_id, "timeRangeChanged", callback); });
    function("clear", +[]() { default_panel(); clear(default_panel_id); });
}


int main(int argc, char** argv)
{
    // Plots draw on the GPU (QRhiWidget). Without this, a panel's first plot makes Qt recreate
    // its window for GPU composition: the new window opens on the primary screen instead of the
    // panel's container, and widgets of the old one keep a dangling QRhi.
    qputenv("QT_WIDGETS_RHI", "1");
    QApplication app(argc, argv);
    // A page may remove all its panels and make new ones later. Quitting on the last one
    // would abort the module (there is no event loop to return to in the browser).
    app.setQuitOnLastWindowClosed(false);
    app.installEventFilter(new OwnContextFirst(&app));
    return app.exec();
}

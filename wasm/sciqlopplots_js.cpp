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

int keep(QObject* object)
{
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
        list.append(QColor(QString::fromStdString(s)));
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
        // Now, not deleteLater(): the page removes the panel's screen right after this call.
        return delete panel;
    }
    object->deleteLater();
}

// options: labels (string[]), colors (CSS colour strings). The first API passed labels alone.
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
            (theme->*setter)(QColor(qstring(color)));
    if (auto* previous = panel->theme())
        previous->deleteLater();
    panel->set_theme(theme);
    QPalette palette(theme->background());   // derives the shades splitters and frames use
    palette.setColor(QPalette::Base, theme->background());
    for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        palette.setColor(role, theme->foreground());
    panel->setPalette(palette);
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
        QColor(qstring(option(options, "color", val("rgba(100, 100, 220, 0.25)")))),
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

// Events: a panel's "timeRangeChanged" (start, stop); a span's "rangeChanged" (start, stop),
// "selectionChanged" (selected) and "deleteRequested" ().
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
    function("line", &line);
    function("scatter", &scatter);
    function("colormap", &colormap_with);
    function("colormap", &colormap);
    function("setData", &set_data_xy);
    function("setData", &set_data_xyz);
    function("setVisible", &set_visible);
    function("setAxis", &set_axis);
    function("axisRange", &axis_range);
    function("setTimeRange", &set_time_range);
    function("timeRange", &time_range);
    function("setTheme", &set_theme);
    function("clear", &clear);
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
    app.installEventFilter(new OwnContextFirst(&app));
    return app.exec();
}

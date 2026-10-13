// SciQLopPlots for JavaScript: one SciQLopMultiPlotPanel filling the page, time plots
// stacked on a shared time axis (seconds since 1970). JavaScript owns the data: it pushes
// arrays with line/colormap/setData and refetches when onTimeRangeChanged fires.
//
//   const spz = await qtLoad({ qt: { entryFunction: sciqlopplots_entry, containerElements: [div] } });
//   spz.setTimeRange(start, stop);   // first: a new plot fits its y axis to the data in view
//   const plot = spz.addPlot();
//   const t = spz.Buffer.fromArray(new Float64Array([...]), [n]);
//   const id = spz.line(plot, t, spz.Buffer.fromArray(values, [n, 3]), ["x", "y", "z"]);
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/SciQLopTheme.hpp"

#include <QApplication>
#include <QPalette>
#include <QPointer>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <map>
#include <type_traits>

using emscripten::val;

namespace
{

SciQLopMultiPlotPanel* panel = nullptr;
std::vector<QPointer<SciQLopPlotInterface>> plots;
std::vector<QPointer<SciQLopPlottableInterface>> plottables;

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
    const auto bytes = val::global("Uint8Array")
                           .new_(array["buffer"], array["byteOffset"], array["byteLength"]);
    return SciQLopPyBuffer(emscripten::convertJSArrayToNumberVector<unsigned char>(bytes),
                           format->second.first, format->second.second,
                           emscripten::vecFromJSArray<std::size_t>(shape));
}

template <typename T>
int keep(std::vector<QPointer<T>>& items, std::type_identity_t<T>* item)
{
    items.emplace_back(item);
    return int(items.size()) - 1;
}

template <typename T>
T* lookup(const std::vector<QPointer<T>>& items, int id)
{
    if (id < 0 || id >= int(items.size()) || items[id].isNull())
        throw std::out_of_range("no such plot or plottable: " + std::to_string(id));
    return items[id];
}

int add_plot()
{
    return keep(plots, panel->create_plot(-1, PlotType::TimeSeries));
}

QStringList string_list(val strings)
{
    QStringList list;
    for (const auto& s : emscripten::vecFromJSArray<std::string>(strings))
        list.append(QString::fromStdString(s));
    return list;
}

int line(int plot, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, val labels)
{
    return keep(plottables, lookup(plots, plot)->line(x, y, string_list(labels)));
}

int colormap(int plot, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
             const SciQLopPyBuffer& z, std::string name, bool y_log, bool z_log)
{
    return keep(plottables, lookup(plots, plot)->colormap(x, y, z, QString::fromStdString(name),
                                                          y_log, z_log));
}

void set_y_log(int plot, bool log)
{
    lookup(plots, plot)->y_axis()->set_log(log);
}

void set_y_label(int plot, std::string label)
{
    lookup(plots, plot)->y_axis()->set_label(QString::fromStdString(label));
}

void set_line_data(int id, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y)
{
    lookup(plottables, id)->set_data(x, y);
}

void set_colormap_data(int id, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                       const SciQLopPyBuffer& z)
{
    lookup(plottables, id)->set_data(x, y, z);
}

void set_time_range(double start, double stop)
{
    panel->set_time_axis_range(start, stop);
}

// The theme colours the plots; the palette, the panel's widgets around them.
void apply_palette(const SciQLopTheme* theme)
{
    QPalette palette(theme->background());   // derives the shades splitters and frames use
    palette.setColor(QPalette::Base, theme->background());
    for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        palette.setColor(role, theme->foreground());
    qApp->setPalette(palette);
    panel->setPalette(palette);
}

// base: "light" or "dark"; colors: any of theme_colors' names, as CSS colour strings.
void set_theme(std::string base, val colors)
{
    auto* theme = base == "dark" ? SciQLopTheme::dark(panel) : SciQLopTheme::light(panel);
    for (const auto& [name, setter] : theme_colors)
        if (colors.hasOwnProperty(name.c_str()))
            (theme->*setter)(QColor(QString::fromStdString(colors[name].as<std::string>())));
    if (auto* previous = panel->theme())
        previous->deleteLater();
    panel->set_theme(theme);
    apply_palette(theme);
}

void on_time_range_changed(val callback)
{
    QObject::connect(panel, &SciQLopPlotPanelInterface::time_range_changed, panel,
                     [callback](SciQLopPlotRange range)
                     { callback(range.start(), range.stop()); });
}

void clear()
{
    panel->clear();
    plots.clear();
    plottables.clear();
}

} // namespace

EMSCRIPTEN_BINDINGS(sciqlopplots)
{
    emscripten::class_<SciQLopPyBuffer>("Buffer")
        .class_function("fromArray", &buffer_from_array)
        .function("size", &SciQLopPyBuffer::flat_size);
    emscripten::function("addPlot", &add_plot);
    emscripten::function("line", &line);
    emscripten::function("colormap", &colormap);
    emscripten::function("setLineData", &set_line_data);
    emscripten::function("setColormapData", &set_colormap_data);
    emscripten::function("setYLog", &set_y_log);
    emscripten::function("setYLabel", &set_y_label);
    emscripten::function("setTheme", &set_theme);
    emscripten::function("setTimeRange", &set_time_range);
    emscripten::function("onTimeRangeChanged", &on_time_range_changed);
    emscripten::function("clear", &clear);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    panel = new SciQLopMultiPlotPanel(nullptr, false, true);
    // Qt for wasm wraps a framed window in an HTML frame with a light grey background, which
    // shows through the gaps between plots.
    panel->setWindowFlag(Qt::FramelessWindowHint);
    panel->setAutoFillBackground(true);
    panel->setFrameShape(QFrame::NoFrame);   // the page draws its own border
    panel->setWindowState(Qt::WindowFullScreen);
    panel->show();
    return app.exec();
}

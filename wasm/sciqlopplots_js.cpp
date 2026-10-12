// SciQLopPlots for JavaScript: one SciQLopMultiPlotPanel filling the page, time plots
// stacked on a shared time axis (seconds since 1970). JavaScript owns the data: it pushes
// arrays with line/colormap/setData and refetches when onTimeRangeChanged fires.
//
//   const spz = await qtLoad({ qt: { entryFunction: sciqlopplots_entry, containerElements: [div] } });
//   spz.setTimeRange(start, stop);   // first: a new plot fits its y axis to the data in view
//   const t = spz.Buffer.fromArray(new Float64Array([...]), [n]);
//   const id = spz.line(t, spz.Buffer.fromArray(values, [n, 3]), ["x", "y", "z"]);
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"

#include <QApplication>
#include <QPointer>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <map>

using emscripten::val;

namespace
{

SciQLopMultiPlotPanel* panel = nullptr;
std::vector<QPointer<SciQLopPlottableInterface>> plottables;

// Buffer-protocol format code and item size of each typed array kind.
const std::map<std::string, std::pair<char, std::size_t>> typed_array_formats {
    { "Float64Array", { 'd', 8 } }, { "Float32Array", { 'f', 4 } },
    { "Int32Array", { 'i', 4 } },   { "Uint32Array", { 'I', 4 } },
    { "Int16Array", { 'h', 2 } },   { "Uint16Array", { 'H', 2 } },
    { "Int8Array", { 'b', 1 } },    { "Uint8Array", { 'B', 1 } },
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

int keep(SciQLopPlottableInterface* plottable)
{
    plottables.emplace_back(plottable);
    return int(plottables.size()) - 1;
}

SciQLopPlotInterface* new_time_plot()
{
    return panel->create_plot(-1, PlotType::TimeSeries);
}

QStringList string_list(val strings)
{
    QStringList list;
    for (const auto& s : emscripten::vecFromJSArray<std::string>(strings))
        list.append(QString::fromStdString(s));
    return list;
}

int line(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, val labels)
{
    return keep(new_time_plot()->line(x, y, string_list(labels)));
}

int colormap(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, const SciQLopPyBuffer& z,
             std::string name, bool y_log, bool z_log)
{
    return keep(
        new_time_plot()->colormap(x, y, z, QString::fromStdString(name), y_log, z_log));
}

SciQLopPlottableInterface* plottable(int id)
{
    if (id < 0 || id >= int(plottables.size()) || plottables[id].isNull())
        throw std::out_of_range("no plottable " + std::to_string(id));
    return plottables[id];
}

void set_line_data(int id, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y)
{
    plottable(id)->set_data(x, y);
}

void set_colormap_data(int id, const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                       const SciQLopPyBuffer& z)
{
    plottable(id)->set_data(x, y, z);
}

void set_time_range(double start, double stop)
{
    panel->set_time_axis_range(start, stop);
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
    plottables.clear();
}

} // namespace

EMSCRIPTEN_BINDINGS(sciqlopplots)
{
    emscripten::class_<SciQLopPyBuffer>("Buffer")
        .class_function("fromArray", &buffer_from_array)
        .function("size", &SciQLopPyBuffer::flat_size);
    emscripten::function("line", &line);
    emscripten::function("colormap", &colormap);
    emscripten::function("setLineData", &set_line_data);
    emscripten::function("setColormapData", &set_colormap_data);
    emscripten::function("setTimeRange", &set_time_range);
    emscripten::function("onTimeRangeChanged", &on_time_range_changed);
    emscripten::function("clear", &clear);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    panel = new SciQLopMultiPlotPanel(nullptr, false, true);
    panel->setWindowState(Qt::WindowFullScreen);
    panel->show();
    return app.exec();
}

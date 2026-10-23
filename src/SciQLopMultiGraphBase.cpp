#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponent.hpp"
#include "SciQLopPlots/Python/Validation.hpp"
#include "SciQLopPlots/Profiling.hpp"
#include "SciQLopPlots/Tracing.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include <algorithm>
#include <cmath>
#include <datasource/row-major-multi-datasource.h>
#include <datasource/soa-multi-datasource.h>
#include <vector>

SciQLopMultiGraphBase::SciQLopMultiGraphBase(const QString& type_label, QCustomPlot* parent,
                                             SciQLopPlotAxis* key_axis,
                                             SciQLopPlotAxis* value_axis,
                                             const QStringList& labels, QVariantMap metaData)
    : SQPQCPAbstractPlottableWrapper(type_label, metaData, parent)
    , _pendingLabels{labels}
    , _keyAxis{key_axis}
    , _valueAxis{value_axis}
{
}

SciQLopMultiGraphBase::~SciQLopMultiGraphBase() { clear_graphs(); }

void SciQLopMultiGraphBase::create_graphs(const QStringList& labels)
{
    if (_multiGraph)
        clear_graphs();

    _multiGraph = create_multi_graph(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    connect(_multiGraph, &QCPAbstractPlottable::busyChanged,
            this, &SciQLopPlottableInterface::busy_changed);
    if (!labels.isEmpty())
        _pendingLabels = labels;
}

void SciQLopMultiGraphBase::clear_graphs(bool graph_already_removed)
{
    clear_plottables();
    if (_multiGraph && !graph_already_removed)
    {
        auto plot = _multiGraph->parentPlot();
        if (plot && plot->hasPlottable(_multiGraph))
            (void)plot->removePlottable(_multiGraph);
    }
    _multiGraph = nullptr;
}

void SciQLopMultiGraphBase::build_data_source(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y)
{
    const auto* keys = static_cast<const double*>(x.raw_data());
    const int n = static_cast<int>(x.flat_size());

    if (n > 0)
        m_data_range = SciQLopPlotRange(keys[0], keys[n - 1]);
    else
        m_data_range = SciQLopPlotRange();

    // Build the data source with a dataGuard that co-owns the PyBuffers,
    // preventing use-after-free when the async pipeline thread reads while
    // new data arrives and replaces _x/_y.
    struct Buffers { SciQLopPyBuffer x, y; };
    auto guard = std::make_shared<Buffers>(Buffers{x, y});

    dispatch_dtype(y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(y.raw_data());

        if (y.ndim() == 1)
        {
            std::vector<std::span<const V>> columns{std::span<const V>(values, n)};
            auto source = std::make_shared<
                QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
                std::span<const double>(keys, n), std::move(columns), guard);
            _multiGraph->setDataSource(std::move(source));
        }
        else
        {
            const auto n_cols = y.size(1);
            if (y.row_major())
            {
                auto source = std::make_shared<QCPRowMajorMultiDataSource<double, V>>(
                    std::span<const double>(keys, n),
                    values, n, static_cast<int>(n_cols), static_cast<int>(n_cols), guard);
                _multiGraph->setDataSource(std::move(source));
            }
            else
            {
                std::vector<std::span<const V>> columns;
                columns.reserve(n_cols);
                for (std::size_t col = 0; col < n_cols; ++col)
                    columns.emplace_back(values + col * n, n);
                auto source = std::make_shared<
                    QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
                    std::span<const double>(keys, n), std::move(columns), guard);
                _multiGraph->setDataSource(std::move(source));
            }
        }
    });
    _dataHolder = std::move(guard);
}

void SciQLopMultiGraphBase::apply_preset(int column)
{
    const auto it = _column_presets.find(column);
    if (it == _column_presets.end() || column < 0
        || column >= static_cast<int>(plottable_count()))
        return;
    const auto& preset = it->second;
    if (preset.line_style)
        component(column)->set_line_style(*preset.line_style);
    if (preset.marker)
        component(column)->set_marker_shape(*preset.marker);
    if (preset.in_legend)
        _multiGraph->setComponentInLegend(column, *preset.in_legend);
}

void SciQLopMultiGraphBase::apply_presets_to_new_components(int first, int last)
{
    for (int i = first; i < last; ++i)
        apply_preset(i);
}

void SciQLopMultiGraphBase::sync_components()
{
    const int nComponents = _multiGraph->componentCount();
    if (static_cast<int>(plottable_count()) < nComponents)
    {
        if (!_pendingLabels.isEmpty())
            _multiGraph->setComponentNames(_pendingLabels);
        const int old_count = static_cast<int>(plottable_count());
        for (int i = old_count; i < nComponents; ++i)
        {
            auto component = new SciQLopGraphComponent(_multiGraph, i, this);
            if (i < _pendingLabels.size())
                component->set_name(_pendingLabels.value(i));
            _register_component(component);
        }
        apply_presets_to_new_components(old_count, nComponents);
        _pendingLabels.clear();
    }
    else
    {
        // New data with fewer columns shrank QCPMultiGraph's component array:
        // drop the trailing wrappers too — a stale wrapper would index
        // component(i) out of bounds.
        if (static_cast<int>(plottable_count()) > nComponents)
        {
            _drop_components_from(nComponents);
            Q_EMIT this->component_list_changed();
        }
    }
}

void SciQLopMultiGraphBase::set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
{
    PROFILE_HERE_N("setdata.multigraph");
    ::SciQLopPlots::tracing::ScopedZone _sz("setdata.multigraph", "setdata");
    _sz.add_arg("n_points", static_cast<int64_t>(x.flat_size()));
    if (!_multiGraph || !x.is_valid() || !y.is_valid())
        return;

    if (x.format_code() != 'd')
        throw std::runtime_error("Keys (x) must be float64");
    // Length/ndim/dtype guard: y spans are built with x's length below, so a
    // shorter y would be read out of bounds at render time.
    sqp::validation::validate_xy(x, y);

    _x = x;
    _y = y;

    build_data_source(x, y);
    sync_components();

    Q_EMIT this->replot();
    check_first_data(static_cast<int>(x.flat_size()));
    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

QList<SciQLopPyBuffer> SciQLopMultiGraphBase::data() const noexcept
{
    return {_x, _y};
}

void SciQLopMultiGraphBase::collect_visible_values(const SciQLopPlotRange& visible_key_range,
                                                   std::vector<double>& out) const noexcept
{
    if (!_x.is_valid() || !_y.is_valid())
        return;
    if (_x.format_code() != 'd')
        return;
    const std::size_t n = _x.flat_size();
    if (n == 0)
        return;

    const double x_lo = visible_key_range.first;
    const double x_hi = visible_key_range.second;
    const double* xs = _x.data();
    const std::size_t k = (_y.ndim() == 1) ? 1 : _y.shape()[1];
    const bool row_major = (_y.ndim() == 1) || _y.row_major();

    // x is sorted (the NeoQCP data sources binary-search the same keys):
    // bound the scan to the visible window and reserve only what it can hold —
    // a full-dataset reserve allocated hundreds of MB when zoomed far in.
    const std::size_t i0 = std::lower_bound(xs, xs + n, x_lo) - xs;
    const std::size_t i1 = std::upper_bound(xs + i0, xs + n, x_hi) - xs;
    if (i0 >= i1)
        return;

    out.reserve(out.size() + (i1 - i0) * k);
    try
    {
        dispatch_dtype(_y.format_code(), [&](auto tag) {
            using V = typename decltype(tag)::type;
            const auto* ys = static_cast<const V*>(_y.raw_data());
            for (std::size_t i = i0; i < i1; ++i)
            {
                for (std::size_t j = 0; j < k; ++j)
                {
                    const double v = static_cast<double>(
                        row_major ? ys[i * k + j] : ys[j * n + i]);
                    if constexpr (std::is_floating_point_v<V>)
                    {
                        if (!std::isfinite(v))
                            continue;
                    }
                    out.push_back(v);
                }
            }
        });
    }
    catch (const std::invalid_argument&) { /* unsupported dtype — skip */ }
}

void SciQLopMultiGraphBase::set_gap_threshold(double threshold)
{
    SCIQLOP_ON_OWNER_THREAD(set_gap_threshold(threshold));
    if (!_multiGraph)
        return;
    _multiGraph->setGapThreshold(threshold);
    Q_EMIT this->replot();
}

double SciQLopMultiGraphBase::gap_threshold() const noexcept
{
    return _multiGraph ? _multiGraph->gapThreshold() : qcp::algo::kDefaultGapThreshold;
}

void SciQLopMultiGraphBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setKeyAxis(a); });
}

void SciQLopMultiGraphBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setValueAxis(a); });
}

namespace
{
void require(bool accepted, const std::string& what)
{
    if (!accepted)
        throw std::invalid_argument(what);
}

// The component's own setters emit these too: an open inspector follows either path.
void notify_component(const SciQLopMultiGraphBase& graph, int column,
                      void (SciQLopGraphComponentInterface::*signal)())
{
    if (column >= 0 && column < static_cast<int>(graph.plottable_count()))
        Q_EMIT(graph.component(column)->*signal)();
}
} // namespace

void SciQLopMultiGraphBase::set_fill_to_value(int column, double value)
{
    sqp::validation::validate_finite(value, "set_fill_to_value");
    SCIQLOP_ON_OWNER_THREAD(set_fill_to_value(column, value));
    require(_multiGraph && _multiGraph->setComponentFill(column, value),
            "set_fill_to_value: invalid column " + std::to_string(column));
    notify_component(*this, column, &SciQLopGraphComponentInterface::fill_changed);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::set_fill_between(int column, int other)
{
    SCIQLOP_ON_OWNER_THREAD(set_fill_between(column, other));
    require(_multiGraph && _multiGraph->setComponentFillBetween(column, other),
            "set_fill_between: invalid columns " + std::to_string(column) + ", "
                + std::to_string(other));
    notify_component(*this, column, &SciQLopGraphComponentInterface::fill_changed);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::clear_fill(int column)
{
    SCIQLOP_ON_OWNER_THREAD(clear_fill(column));
    if (!_multiGraph)
        return;
    _multiGraph->clearComponentFill(column);
    notify_component(*this, column, &SciQLopGraphComponentInterface::fill_changed);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::set_fill_color(int column, const QColor& color)
{
    SCIQLOP_ON_OWNER_THREAD(set_fill_color(column, color));
    if (!_multiGraph)
        return;
    require(_multiGraph->componentFill(column).has_value(),
            "column " + std::to_string(column) + " has no fill");
    _multiGraph->setComponentFillBrush(column, QBrush(color));
    notify_component(*this, column, &SciQLopGraphComponentInterface::fill_changed);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::set_error_bars(int column, int lower, int upper, double cap_width)
{
    require(cap_width >= 0, "set_error_bars: cap_width must be >= 0");
    SCIQLOP_ON_OWNER_THREAD(set_error_bars(column, lower, upper, cap_width));
    require(_multiGraph && _multiGraph->setComponentErrorBars(column, lower, upper, cap_width),
            "set_error_bars: invalid columns " + std::to_string(column) + ", "
                + std::to_string(lower) + ", " + std::to_string(upper));
    notify_component(*this, column, &SciQLopGraphComponentInterface::error_bars_changed);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::clear_error_bars(int column)
{
    SCIQLOP_ON_OWNER_THREAD(clear_error_bars(column));
    if (!_multiGraph)
        return;
    _multiGraph->clearComponentErrorBars(column);
    notify_component(*this, column, &SciQLopGraphComponentInterface::error_bars_changed);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::set_column_line_style(int column, GraphLineStyle style)
{
    require(column >= 0, "set_column_line_style: invalid column " + std::to_string(column));
    SCIQLOP_ON_OWNER_THREAD(set_column_line_style(column, style));
    _column_presets[column].line_style = style;
    apply_preset(column);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::set_column_marker_shape(int column, GraphMarkerShape shape)
{
    require(column >= 0, "set_column_marker_shape: invalid column " + std::to_string(column));
    SCIQLOP_ON_OWNER_THREAD(set_column_marker_shape(column, shape));
    _column_presets[column].marker = shape;
    apply_preset(column);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::set_column_in_legend(int column, bool in_legend)
{
    require(column >= 0, "set_column_in_legend: invalid column " + std::to_string(column));
    SCIQLOP_ON_OWNER_THREAD(set_column_in_legend(column, in_legend));
    _column_presets[column].in_legend = in_legend;
    apply_preset(column);
    Q_EMIT replot();
}

bool SciQLopMultiGraphBase::column_in_legend(int column) const
{
    const auto it = _column_presets.find(column);
    if (it != _column_presets.end() && it->second.in_legend)
        return *it->second.in_legend;
    if (_multiGraph && column >= 0 && column < _multiGraph->componentCount())
        return _multiGraph->component(column).inLegend;
    return true;
}

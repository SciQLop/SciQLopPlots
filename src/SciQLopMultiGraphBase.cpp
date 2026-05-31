#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponent.hpp"
#include "SciQLopPlots/Profiling.hpp"
#include "SciQLopPlots/Tracing.hpp"
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
        if (plot)
            plot->removePlottable(_multiGraph);
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

void SciQLopMultiGraphBase::sync_components()
{
    const int nComponents = _multiGraph->componentCount();
    if (static_cast<int>(plottable_count()) < nComponents)
    {
        if (!_pendingLabels.isEmpty())
            _multiGraph->setComponentNames(_pendingLabels);
        for (int i = static_cast<int>(plottable_count()); i < nComponents; ++i)
        {
            auto component = new SciQLopGraphComponent(_multiGraph, i, this);
            if (i < _pendingLabels.size())
                component->set_name(_pendingLabels.value(i));
            _register_component(component);
        }
        _pendingLabels.clear();
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

    out.reserve(out.size() + n * k);
    try
    {
        dispatch_dtype(_y.format_code(), [&](auto tag) {
            using V = typename decltype(tag)::type;
            const auto* ys = static_cast<const V*>(_y.raw_data());
            for (std::size_t i = 0; i < n; ++i)
            {
                const double x = xs[i];
                if (x < x_lo || x > x_hi)
                    continue;
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

void SciQLopMultiGraphBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setKeyAxis(a); });
}

void SciQLopMultiGraphBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setValueAxis(a); });
}

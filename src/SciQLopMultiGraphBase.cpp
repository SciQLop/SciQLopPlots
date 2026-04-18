#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponent.hpp"
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

void SciQLopMultiGraphBase::build_data_source(const PyBuffer& x, const PyBuffer& y)
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
    struct Buffers { PyBuffer x, y; };
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

void SciQLopMultiGraphBase::set_data(PyBuffer x, PyBuffer y)
{
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

QList<PyBuffer> SciQLopMultiGraphBase::data() const noexcept
{
    return {_x, _y};
}

void SciQLopMultiGraphBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setKeyAxis(a); });
}

void SciQLopMultiGraphBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setValueAxis(a); });
}

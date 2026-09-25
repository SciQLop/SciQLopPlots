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
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Plotables/SciQLopCurve.hpp"
#include "SciQLopPlots/Plotables/SciQLopTimeColoredCurve.hpp"
#include "SciQLopPlots/Plotables/Resamplers/SciQLopCurveResampler.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/Python/Validation.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

void SciQLopCurve::_setCurveData(QList<QVector<QCPCurveData>> data)
{
    // A curve built without labels starts with no components and leaves the
    // resampler's line count at 0, which makes it emit every column the data
    // holds. Size ourselves from that batch — the same "components follow the
    // data" contract as SciQLopMultiGraphBase::sync_components. A labelled curve
    // keeps its component count as a contract (set_data validates the column
    // count against it), so it is left alone here.
    if (_resampler->line_count() == 0)
        _sync_component_count(static_cast<int>(data.size()));

    // The resampler emits via QueuedConnection, so the component count may
    // have grown or shrunk between emit and delivery. Cap iteration to the
    // smaller of the two to avoid OOB indexing into `data`.
    const std::size_t n = std::min<std::size_t>(plottable_count(),
                                                static_cast<std::size_t>(data.size()));
    for (std::size_t i = 0; i < n; i++)
    {
        auto curve = line(i);
        if (curve)
            curve->data()->set(data[i], true);
    }
    _resampler_busy = false;
    set_busy(false);
    Q_EMIT this->replot();
    Q_EMIT data_changed();
}

void SciQLopCurve::clear_curves(bool curve_already_removed)
{
    clear_plottables();
}

// No join: quit() only takes effect once the running resample returns, which would freeze
// the GUI thread that destroys us for as long as it lasts (#109). The resampler and its
// thread free themselves when the thread finishes; the late result goes nowhere.
void SciQLopCurve::clear_resampler()
{
    disconnect(this->_resampler, nullptr, this, nullptr);
    connect(this->_resampler_thread, &QThread::finished, this->_resampler, &QObject::deleteLater);
    connect(this->_resampler_thread, &QThread::finished, this->_resampler_thread,
            &QObject::deleteLater);
    this->_resampler_thread->quit();
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopCurve::create_resampler(const QStringList& labels)
{
    this->_resampler = new CurveResampler(this, std::size(labels));
    this->_resampler_thread = new QThread();
    this->_resampler_thread->setObjectName(QStringLiteral("curveRsmpl"));
    this->_resampler->moveToThread(this->_resampler_thread);
    this->_resampler_thread->start(QThread::LowPriority);
    connect(this->_resampler, &CurveResampler::setGraphData, this, &SciQLopCurve::_setCurveData,
            Qt::QueuedConnection);
}

SciQLopCurve::SciQLopCurve(QCustomPlot* parent, SciQLopPlotAxis* keyAxis,
                           SciQLopPlotAxis* valueAxis, const QStringList& labels, QVariantMap metaData)
        : SQPQCPAbstractPlottableWrapper("Curve",metaData, parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
{
    create_resampler(labels);
    this->create_graphs(labels);
}

SciQLopCurve::SciQLopCurve(QCustomPlot* parent, SciQLopPlotAxis* keyAxis,
                           SciQLopPlotAxis* valueAxis, QVariantMap metaData)
        : SQPQCPAbstractPlottableWrapper("Curve", metaData, parent)
        , _keyAxis { keyAxis }
        , _valueAxis { valueAxis }
{
    create_resampler({});
}

SciQLopCurve::~SciQLopCurve()
{
    // Still listed among the plot's plottables here, so let it look again once we are gone.
    notify_color_scale_later();
    clear_curves();
    clear_resampler();
}

void SciQLopCurve::set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
{
    // The resampler converts x/y from any numeric dtype to double. Reject
    // unsupported dtypes here (at the Python boundary) so the worker thread only
    // ever sees dtypes dispatch_dtype can handle.
    const auto require_numeric = [](const SciQLopPyBuffer& buf, const char* name)
    {
        if (!buf.is_valid())
            return;
        try
        {
            dispatch_dtype(buf.format_code(), [](auto) {});
        }
        catch (const std::invalid_argument&)
        {
            throw std::runtime_error(std::string("Curve.set_data: ") + name
                                     + " has an unsupported (non-numeric) dtype");
        }
    };
    require_numeric(x, "x");
    require_numeric(y, "y");
    if (x.is_valid() && y.is_valid())
    {
        sqp::validation::validate_xy(x, y);
        // A labelled curve's component count is a contract — one y column per
        // label, and short data would have the worker read past the buffer. An
        // unlabelled curve (line_count() == 0, an invariant _sync_component_count
        // preserves) sizes itself from each batch instead, so any column count is
        // legitimate there; the resampler bounds its own reads by the y buffer.
        const auto components = _resampler->line_count();
        if (components > 0 && y.flat_size() != x.flat_size() * components)
            throw std::invalid_argument(
                "y must hold exactly one column per curve component");
    }
    _point_count = x.is_valid() ? x.flat_size() : 0;
    // The resampler early-returns without emitting setGraphData when x is
    // invalid/empty or y is invalid (and _setCurveData is the only path that
    // clears busy) — only raise busy when a result will actually come back.
    _resampler_busy = x.is_valid() && y.is_valid() && x.flat_size() > 0;
    if (_resampler_busy)
        set_busy(true);
    this->_resampler->setData(x, y);
    Q_EMIT data_changed(x, y);
}

QList<SciQLopPyBuffer> SciQLopCurve::data() const noexcept
{
    return _resampler->get_data();
}

void SciQLopCurve::collect_visible_values(const SciQLopPlotRange& visible_key_range,
                                          std::vector<double>& out) const noexcept
{
    if (!_resampler)
        return;
    const auto buffers = _resampler->get_data();
    if (buffers.size() < 2)
        return;
    const auto& x = buffers[0];
    const auto& y = buffers[1];
    if (!x.is_valid() || !y.is_valid())
        return;
    const std::size_t n = x.flat_size();
    if (n == 0 || y.flat_size() < n)
        return;

    const double x_lo = visible_key_range.first;
    const double x_hi = visible_key_range.second;

    // Parametric x is unsorted — the scan stays, but no full-dataset reserve:
    // amortized push_back growth beats a guaranteed n-sized allocation when
    // only a fraction of the trajectory is visible.
    try
    {
        dispatch_dtype(x.format_code(), [&](auto x_tag) {
        dispatch_dtype(y.format_code(), [&](auto y_tag) {
            using XT = typename decltype(x_tag)::type;
            using V = typename decltype(y_tag)::type;
            const auto* xs = static_cast<const XT*>(x.raw_data());
            const auto* ys = static_cast<const V*>(y.raw_data());
            for (std::size_t i = 0; i < n; ++i)
            {
                const double xv = static_cast<double>(xs[i]);
                if (xv < x_lo || xv > x_hi)
                    continue;
                const double v = static_cast<double>(ys[i]);
                if constexpr (std::is_floating_point_v<V>)
                {
                    if (!std::isfinite(v))
                        continue;
                }
                out.push_back(v);
            }
        });
        });
    }
    catch (const std::invalid_argument&) { /* unsupported dtype — skip */ }
}

void SciQLopCurve::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) {
        for (auto p : m_components)
            qobject_cast<QCPCurve*>(p->plottable())->setKeyAxis(a);
    });
}

void SciQLopCurve::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) {
        for (auto p : m_components)
            qobject_cast<QCPCurve*>(p->plottable())->setValueAxis(a);
    });
}

void SciQLopCurve::_sync_component_count(int count)
{
    if (static_cast<int>(plottable_count()) == count)
        return;

    // Fewer columns than last time: a stale wrapper would keep drawing the
    // previous batch's curve.
    while (static_cast<int>(plottable_count()) > count)
        delete m_components.takeLast().data();

    for (int i = static_cast<int>(plottable_count()); i < count; ++i)
        newComponent<SciQLopTimeColoredCurve>(_keyAxis->qcp_axis(), _valueAxis->qcp_axis(),
                                              QString());

    // The first component carries the busy flag for the whole graph; it changes
    // identity whenever the list was emptied, so re-arm (UniqueConnection makes
    // the common no-op case free).
    if (!m_components.isEmpty())
        if (auto* p = m_components.first()->plottable())
            connect(p, &QCPAbstractPlottable::busyChanged, this,
                    &SciQLopPlottableInterface::busy_changed, Qt::UniqueConnection);

    Q_EMIT this->component_list_changed();
}

void SciQLopCurve::create_graphs(const QStringList& labels)
{
    if (plottable_count())
        clear_curves();
    for (const auto& label : labels)
    {
        this->newComponent<SciQLopTimeColoredCurve>(_keyAxis->qcp_axis(), _valueAxis->qcp_axis(), label);
    }
    if (!m_components.isEmpty())
    {
        if (auto p = m_components.first()->plottable())
            connect(p, &QCPAbstractPlottable::busyChanged,
                    this, &SciQLopPlottableInterface::busy_changed);
    }
    _resampler->set_line_count(plottable_count());
}

void SciQLopCurve::set_time_color_enabled(bool enabled)
{
    for (auto comp : m_components)
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(comp->plottable()))
            tc->set_time_color_enabled(enabled);
}

bool SciQLopCurve::time_color_enabled() const
{
    if (!m_components.isEmpty())
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(m_components.first()->plottable()))
            return tc->time_color_enabled();
    return false;
}

void SciQLopCurve::set_time_values(const QVector<double>& times)
{
    for (auto comp : m_components)
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(comp->plottable()))
            tc->set_time_values(times);
}

void SciQLopCurve::set_color_values(const QVector<double>& values)
{
    for (auto comp : m_components)
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(comp->plottable()))
            tc->set_color_values(values);
}

void SciQLopCurve::set_time_color_gradient(const QColor& start, const QColor& end)
{
    for (auto comp : m_components)
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(comp->plottable()))
            tc->set_gradient_colors(start, end);
}

void SciQLopCurve::set_color_gradient(::ColorGradient gradient)
{
    const QCPColorGradient qcp_gradient { to_qcp(gradient) };
    for (auto comp : m_components)
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(comp->plottable()))
            tc->set_color_gradient(qcp_gradient);
    notify_color_scale(gradient);
    Q_EMIT this->replot();
}

void SciQLopCurve::set_visible(bool visible) noexcept
{
    SQPQCPAbstractPlottableWrapper::set_visible(visible);
    notify_color_scale();
}

void SciQLopCurve::set_color_scale(QCPColorScale* scale)
{
    for (auto comp : m_components)
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(comp->plottable()))
            tc->set_color_scale(scale);
}

bool SciQLopCurve::has_color_values() const
{
    if (!m_components.isEmpty())
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(m_components.first()->plottable()))
            return tc->has_color_values();
    return false;
}

std::optional<std::pair<double, double>> SciQLopCurve::color_range(bool log) const
{
    if (!m_components.isEmpty())
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(m_components.first()->plottable()))
            return tc->color_range(log);
    return std::nullopt;
}

void SciQLopCurve::set_line_width(qreal width)
{
    for (auto comp : m_components)
        comp->set_line_width(width);
    Q_EMIT this->replot();
}

qreal SciQLopCurve::line_width() const
{
    return m_components.isEmpty() ? 1.0 : m_components.first()->line_width();
}

void SciQLopCurve::set_color_data(SciQLopPyBuffer values, ::ColorGradient gradient)
{
    QVector<double> colors;
    if (values.is_valid() && values.flat_size() > 0)
    {
        if (values.flat_size() != _point_count)
            throw std::invalid_argument(
                "Curve.set_color_data: expected one colour value per data point ("
                + std::to_string(_point_count) + "), got "
                + std::to_string(values.flat_size()));

        colors = to_double_vector<QVector<double>>(values);
    }

    const QCPColorGradient qcp_gradient { to_qcp(gradient) };
    for (auto comp : m_components)
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(comp->plottable()))
            tc->set_color_gradient(qcp_gradient);

    set_color_values(colors);
    set_time_color_enabled(!colors.isEmpty());
    notify_color_scale(colors.isEmpty() ? std::nullopt : std::optional { gradient });
    Q_EMIT this->replot();
}

void SciQLopCurve::set_data_and_color(const QList<SciQLopPyBuffer>& data,
                                      const SciQLopPyBuffer& color)
{
    if (data.size() != 2)
        throw std::invalid_argument("Curve: a coloured batch needs [x, y], got "
                                    + std::to_string(data.size()) + " buffers");
    const std::size_t samples = data[0].is_valid() ? data[0].flat_size() : 0;
    const std::size_t count = color.is_valid() ? color.flat_size() : 0;
    if (count > 0 && count != samples)
        throw std::invalid_argument("Curve: expected one colour value per data point ("
                                    + std::to_string(samples) + "), got "
                                    + std::to_string(count));
    set_data(data[0], data[1]);
    const auto colors = count > 0 ? to_double_vector<QVector<double>>(color) : QVector<double> {};
    set_color_values(colors);
    set_time_color_enabled(!colors.isEmpty());
    notify_color_scale();
    Q_EMIT this->replot();
}

QVariant SciQLopCurve::position_at_time(double t) const
{
    if (!m_components.isEmpty())
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(m_components.first()->plottable()))
            if (const auto point = tc->position_at_time(t))
                return QVariant::fromValue(*point);
    return {};
}

SciQLopCurveFunction::SciQLopCurveFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                           SciQLopPlotAxis* value_axis,
                                           GetDataPyCallable&& callable, const QStringList& labels, QVariantMap metaData)
        : SciQLopCurve { parent, key_axis, value_axis, labels,metaData }
        , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    // Curve manages busy via its own resampler (_setCurveData clears it), so
    // the plain pipeline_idle fallback would clear busy while a fetched batch
    // is still being resampled. But a failing callable produces no data at
    // all, leaving busy stuck — clear on idle only when no resample is pending.
    QObject::disconnect(m_idle_connection);
    m_idle_connection = QObject::connect(m_pipeline, &SimplePyCallablePipeline::pipeline_idle,
                     this, [this]()
                     {
                         if (!_resampler_busy)
                             set_busy(false);
                     });
    this->set_range({ parent->xAxis->range().lower, parent->xAxis->range().upper });
}

SciQLopCurveRemote::SciQLopCurveRemote(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                       SciQLopPlotAxis* value_axis,
                                       const QStringList& labels, QVariantMap metaData)
    : SciQLopCurve{parent, key_axis, value_axis, labels, std::move(metaData)}
    , SciQLopRemoteGraph(this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}

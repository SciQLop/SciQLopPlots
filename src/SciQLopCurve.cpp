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
#include <cmath>

void SciQLopCurve::_range_changed(const QCPRange& newRange, const QCPRange& oldRange)
{
    this->_resampler->resample(newRange);
}

void SciQLopCurve::_setCurveData(QList<QVector<QCPCurveData>> data)
{
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
    set_busy(false);
    Q_EMIT this->replot();
    Q_EMIT data_changed();
}

void SciQLopCurve::clear_curves(bool curve_already_removed)
{
    clear_plottables();
}

void SciQLopCurve::clear_resampler()
{
    disconnect(this->_resampler, &CurveResampler::setGraphData, this, &SciQLopCurve::_setCurveData);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler;
    delete this->_resampler_thread;
    this->_resampler = nullptr;
    this->_resampler_thread = nullptr;
}

void SciQLopCurve::create_resampler(const QStringList& labels)
{
    this->_resampler = new CurveResampler(this, std::size(labels));
    this->_resampler_thread = new QThread();
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
    clear_curves();
    clear_resampler();
}

void SciQLopCurve::set_data(PyBuffer x, PyBuffer y)
{
    // CurveResampler reads y via PyBuffer::data() which only returns a valid
    // double* for float64 buffers (see PythonInterface.hpp). Other dtypes
    // would misrender silently — fail loudly instead, matching what
    // SciQLopSingleLineGraph / SciQLopMultiGraphBase do for x.
    if (x.is_valid() && x.format_code() != 'd')
        throw std::runtime_error("Curve.set_data: x must be float64");
    if (y.is_valid() && y.format_code() != 'd')
        throw std::runtime_error("Curve.set_data: y must be float64");
    set_busy(true);
    this->_resampler->setData(x, y);
    Q_EMIT data_changed(x, y);
}

QList<PyBuffer> SciQLopCurve::data() const noexcept
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
    if (x.format_code() != 'd')
        return;
    const std::size_t n = x.flat_size();
    if (n == 0 || y.flat_size() < n)
        return;

    const double x_lo = visible_key_range.first;
    const double x_hi = visible_key_range.second;
    const double* xs = x.data();

    out.reserve(out.size() + n);
    try
    {
        dispatch_dtype(y.format_code(), [&](auto tag) {
            using V = typename decltype(tag)::type;
            const auto* ys = static_cast<const V*>(y.raw_data());
            for (std::size_t i = 0; i < n; ++i)
            {
                const double xv = xs[i];
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

std::optional<QPointF> SciQLopCurve::position_at_time(double t) const
{
    if (!m_components.isEmpty())
        if (auto* tc = dynamic_cast<SciQLopTimeColoredCurve*>(m_components.first()->plottable()))
            return tc->position_at_time(t);
    return std::nullopt;
}

SciQLopCurveFunction::SciQLopCurveFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                           SciQLopPlotAxis* value_axis,
                                           GetDataPyCallable&& callable, const QStringList& labels, QVariantMap metaData)
        : SciQLopCurve { parent, key_axis, value_axis, labels,metaData }
        , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    // Curve manages busy via its own resampler (_setCurveData clears it)
    QObject::disconnect(m_idle_connection);
    this->set_range({ parent->xAxis->range().lower, parent->xAxis->range().upper });
}

#include "SciQLopPlots/ColorScaleController.hpp"
#include "SciQLopPlots/Plotables/SciQLopTimeColoredCurve.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

ColorScaleController::ColorScaleController(SciQLopPlot* owner, Sources sources, QObject* parent)
        : QObject(parent), m_owner(owner), m_sources(std::move(sources))
{
    // A colormap that owns the scale moves its range around; that is not a pin.
    const auto foreign = [this] { return !m_shown && m_owner->color_scale()->visible(); };
    connect(owner->z_axis(), &SciQLopPlotAxisInterface::range_changed, this,
            [this, foreign](const SciQLopPlotRange&)
            {
                if (m_enabled && !m_updating && !foreign())
                    m_auto_range = false;
            });
    connect(owner->z_axis(), &SciQLopPlotAxisInterface::log_changed, this,
            [this](bool)
            {
                if (m_enabled && m_shown && m_auto_range)
                    update();
            });
}

void ColorScaleController::set_enabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (enabled)
    {
        update();
        return;
    }
    for (const auto& source : m_sources())
        source.attach(nullptr);
    hide();
}

void ColorScaleController::set_auto_range(bool enabled)
{
    m_auto_range = enabled;
    if (enabled)
        update();
}

void ColorScaleController::set_gradient(::ColorGradient gradient)
{
    if (!m_enabled)
        return;
    m_gradient_chosen = true;
    if (auto* axis = qobject_cast<SciQLopPlotColorScaleAxis*>(m_owner->z_axis()))
        axis->set_color_gradient(gradient);
}

void ColorScaleController::set_gradient_colors(const QColor& start, const QColor& end)
{
    m_start = start;
    m_end = end;
    m_gradient_chosen = true;
    apply_two_stop();
}

void ColorScaleController::apply_two_stop()
{
    if (auto* axis = qobject_cast<SciQLopPlotColorScaleAxis*>(m_owner->z_axis()))
        axis->set_custom_gradient(SciQLopTimeColoredCurve::two_stop_gradient(m_start, m_end));
}

bool ColorScaleController::show()
{
    if (m_shown)
        return true;
    if (m_owner->color_scale()->visible())
        return false;
    m_owner->show_color_scale();
    m_shown = true;
    if (!m_gradient_chosen)
        apply_two_stop();
    return true;
}

void ColorScaleController::hide()
{
    if (!m_shown)
        return;
    m_owner->hide_color_scale();
    m_shown = false;
}

void ColorScaleController::update()
{
    if (!m_enabled)
        return;
    std::vector<Source> coloured;
    for (auto& source : m_sources())
        if (source.has_values())
            coloured.push_back(std::move(source));
    if (coloured.empty())
    {
        hide();
        return;
    }
    if (!show())
        return;
    for (const auto& source : coloured)
        source.attach(m_owner->color_scale());
    if (m_auto_range)
        rescale(coloured);
}

void ColorScaleController::rescale(const std::vector<Source>& coloured)
{
    const bool log = m_owner->z_axis()->log();
    Range range;
    for (const auto& source : coloured)
        if (const auto r = source.range(log))
            range = range ? std::pair { std::min(range->first, r->first),
                                        std::max(range->second, r->second) }
                          : *r;
    if (!range)
        return;
    auto [lo, hi] = *range;
    if (lo >= hi)
    {
        lo = log ? lo / 2 : lo - 0.5;
        hi = log ? hi * 2 : hi + 0.5;
    }
    m_updating = true;
    m_owner->z_axis()->set_range(SciQLopPlotRange(lo, hi));
    m_updating = false;
}

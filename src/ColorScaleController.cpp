#include "SciQLopPlots/ColorScaleController.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopTimeColoredCurve.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

ColorScaleController::ColorScaleController(SciQLopPlot* owner, Sources sources, QObject* parent)
        : QObject(parent), m_owner(owner), m_sources(std::move(sources))
{
    connect(owner->z_axis(), &SciQLopPlotAxisInterface::range_changed, this,
            [this](const SciQLopPlotRange&)
            {
                if (m_enabled && !m_updating && !foreign())
                    m_auto_range = false;
            });
    // Once the plot starts to go, its children are half destroyed: stay out of them.
    // (A plot whose destructor body deletes graphs calls quiesce() first.)
    if (parent)
        connect(parent, &QObject::destroyed, this, [this] { m_dying = true; });
    // A colormap added later takes the scale over from the curves.
    connect(owner, &SciQLopPlotInterface::graph_list_changed, this, [this] { update(); });
    connect(owner->z_axis(), &SciQLopPlotAxisInterface::log_changed, this,
            [this](bool)
            {
                if (m_enabled && m_shown && m_auto_range)
                    update();
            });
}

bool ColorScaleController::hosts_colormap() const
{
    for (auto* plottable : m_owner->plottables())
        if (dynamic_cast<SciQLopColorMapInterface*>(plottable))
            return true;
    return false;
}

//! The scale is a colormap's, not ours: it hosts one, or it was shown by someone else.
bool ColorScaleController::foreign() const
{
    return hosts_colormap() || (!m_shown && m_owner->color_scale()->visible());
}

void ColorScaleController::yield()
{
    if (!m_shown)
        return;
    for (const auto& source : m_sources())
        source.attach(nullptr);
    m_shown = false;
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
    const bool was_visible = m_owner->color_scale()->visible();
    hide();
    if (m_owner->color_scale()->visible() != was_visible)
        Q_EMIT m_owner->graph_list_changed();
}

void ColorScaleController::set_auto_range(bool enabled)
{
    if (!m_enabled)
        return;
    m_auto_range = enabled;
    if (enabled)
        update();
}

void ColorScaleController::request_gradient(::ColorGradient gradient)
{
    if (!foreign())
        set_gradient(gradient);
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
    if (!m_enabled)
        return;
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
    if (!m_enabled || m_dying)
        return;
    const bool was_visible = m_owner->color_scale()->visible();
    refresh();
    // has_colormap() follows the scale's visibility: let the inspector republish its axes.
    if (m_owner->color_scale()->visible() != was_visible)
        Q_EMIT m_owner->graph_list_changed();
}

void ColorScaleController::refresh()
{
    if (hosts_colormap())
    {
        m_had_colormap = true;
        yield();
        return;
    }
    if (m_had_colormap)
    {
        // Its colormap is gone; the bar it left behind is ours to keep or to hide.
        m_had_colormap = false;
        m_shown = m_owner->color_scale()->visible();
    }
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

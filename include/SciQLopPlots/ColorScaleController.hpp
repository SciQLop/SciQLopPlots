/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#pragma once
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"
#include <QColor>
#include <QObject>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

class QCPColorScale;
class SciQLopPlot;

/*!
 * \brief The one colour scale of a plot, shared by the curves coloured by a scalar.
 *
 * It lives in the colour scale of \a owner (a pane). It is shown while some source
 * carries values and hidden when none does. Its range follows the values of all
 * sources until the range is set through the owner's z axis, which pins it. A
 * colormap always wins the scale, whichever came first: a colormap added later takes
 * it over from the curves (they fall back to their own range and gradient), and one
 * that is removed leaves the scale to the curves, or hides it if none is coloured.
 * Hidden sources do not count.
 * See docs/colour-by-scalar-curves-vs-line-graphs.md.
 */
class ColorScaleController : public QObject
{
    Q_OBJECT

public:
    using Range = std::optional<std::pair<double, double>>;
    //! What the controller needs from a coloured graph.
    struct Source
    {
        std::function<bool()> has_values;
        std::function<Range(bool log)> range;
        std::function<void(QCPColorScale*)> attach;
    };
    using Sources = std::function<std::vector<Source>()>;

    ColorScaleController(SciQLopPlot* owner, Sources sources, QObject* parent = nullptr);

    bool enabled() const noexcept { return m_enabled; }
    //! Off: the sources go back to their own range and gradient, and the scale is hidden.
    void set_enabled(bool enabled);

    bool auto_range() const noexcept { return m_auto_range; }
    void set_auto_range(bool enabled);

    //! A gradient set here is kept: the default ramp no longer replaces it.
    void set_gradient(::ColorGradient gradient);
    //! Same, on behalf of a coloured graph: it never touches a scale that a colormap owns.
    void request_gradient(::ColorGradient gradient);
    void set_gradient_colors(const QColor& start, const QColor& end);

    //! For the plot's destructor body, which deletes graphs before this object learns
    //! about it: no more reading of the plot's children.
    void quiesce() noexcept { m_dying = true; }

    //! Re-reads the sources: shows or hides the scale, attaches them, rescales.
    void update();

private:
    bool hosts_colormap() const;
    bool foreign() const;
    //! Lets go of the curves without hiding the scale, which a colormap now uses.
    void yield();
    void refresh();
    void show();
    void hide();
    void rescale(const std::vector<Source>& coloured);
    void apply_two_stop();
    void apply_gradient();
    void reapply_gradient();
    void restore_pin();
    void remember(::ColorGradient gradient);

    SciQLopPlot* m_owner;
    Sources m_sources;
    bool m_enabled = true;
    bool m_shown = false;
    bool m_auto_range = true;
    bool m_updating = false;
    bool m_dying = false;
    bool m_had_colormap = false;
    bool m_gradient_chosen = false;
    std::optional<::ColorGradient> m_preset;
    std::optional<SciQLopPlotRange> m_pinned;
    QColor m_start { 0, 0, 255 };
    QColor m_end { 255, 0, 0 };
};

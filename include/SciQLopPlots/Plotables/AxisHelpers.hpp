#pragma once
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

inline void apply_axis(SciQLopPlotAxis*& stored,
                       SciQLopPlotAxisInterface* axis,
                       auto&& apply_to_plottable)
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        stored = qcp_axis;
        apply_to_plottable(qcp_axis->qcp_axis());
    }
}

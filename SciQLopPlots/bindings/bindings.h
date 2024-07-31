#ifndef SCIQLOPPLOTS_BINDINGS_H
#define SCIQLOPPLOTS_BINDINGS_H
// Prevents shiboken from turning signals into functions
#define BINDINGS_H
#define _CRT_USE_BUILTIN_OFFSETOF
// #define QT_ANNOTATE_ACCESS_SPECIFIER(a) __attribute__((annotate(#a)))
// #include <memory>
#include "SciQLopPlots/Python/BufferProtocol.hpp"

#include "_QCustomPlot.hpp"
#include <SciQLopPlots/Items/SciQLopVerticalSpan.hpp>
#include <SciQLopPlots/MultiPlots/SciQLopMultiPlotObject.hpp>
#include <SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp>
#include <SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp>
#include <SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp>
#include <SciQLopPlots/Plotables/SciQLopColorMap.hpp>
#include <SciQLopPlots/Plotables/SciQLopCurve.hpp>
#include <SciQLopPlots/Plotables/SciQLopGraph.hpp>
#include <SciQLopPlots/SciQLopPlot.hpp>
#include <qcustomplot.h>

#endif // SCIQLOPPLOTS_BINDINGS_H

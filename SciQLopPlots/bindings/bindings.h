#ifndef SCIQLOPPLOTS_BINDINGS_H
#define SCIQLOPPLOTS_BINDINGS_H
// Prevents shiboken from turning signals into functions
#define BINDINGS_H
#define _CRT_USE_BUILTIN_OFFSETOF
// #define QT_ANNOTATE_ACCESS_SPECIFIER(a) __attribute__((annotate(#a)))
// #include <memory>

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "_QCustomPlot.hpp"
#include <SciQLopPlots/DataProducer/DataProducer.hpp>
#include <SciQLopPlots/DragNDrop/PlotDragNDropCallback.hpp>
#include <SciQLopPlots/Inspector/InspectorBase.hpp>
#include <SciQLopPlots/Inspector/Inspectors.hpp>
#include <SciQLopPlots/Inspector/Model/Model.hpp>
#include <SciQLopPlots/Inspector/Model/Node.hpp>
#include <SciQLopPlots/Inspector/PropertyDelegateBase.hpp>
#include <SciQLopPlots/Inspector/View/InspectorView.hpp>
#include <SciQLopPlots/Inspector/View/PropertiesPanel.hpp>
#include <SciQLopPlots/Inspector/View/TreeView.hpp>
#include <SciQLopPlots/Items/SciQLopVerticalSpan.hpp>
#include <SciQLopPlots/Items/SciQLopPixmapItem.hpp>
#include <SciQLopPlots/Items/SciQLopShapesItems.hpp>
#include <SciQLopPlots/Items/SciQLopTextItem.hpp>
#include <SciQLopPlots/Items/SciQLopStraightLines.hpp>
#include <SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp>
#include <SciQLopPlots/MultiPlots/SciQLopMultiPlotObject.hpp>
#include <SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp>
#include <SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp>
#include <SciQLopPlots/MultiPlots/VPlotsAlign.hpp>
#include <SciQLopPlots/MultiPlots/XAxisSynchronizer.hpp>
#include <SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp>
#include <SciQLopPlots/Plotables/SciQLopColorMap.hpp>
#include <SciQLopPlots/Plotables/SciQLopCurve.hpp>
#include <SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp>
#include <SciQLopPlots/Plotables/SciQLopGraphInterface.hpp>
#include <SciQLopPlots/Plotables/SciQLopLineGraph.hpp>
#include <SciQLopPlots/SciQLopPlot.hpp>
#include <SciQLopPlots/SciQLopPlotAxis.hpp>
#include <SciQLopPlots/SciQLopPlotInterface.hpp>
#include <SciQLopPlots/SciQLopPlotRange.hpp>
#include <SciQLopPlots/SciQLopTimeSeriesPlot.hpp>
#include <SciQLopPlots/SciQLopNDProjectionPlot.hpp>
#include <SciQLopPlots/SciQLopPlotLegendInterface.hpp>
#include <SciQLopPlots/SciQLopPlotLegend.hpp>
#include <SciQLopPlots/enums.hpp>

#include <SciQLopPlots/Products/ProductsModel.hpp>
#include <SciQLopPlots/Products/ProductsNode.hpp>
#include <SciQLopPlots/Products/ProductsView.hpp>

#include <SciQLopPlots/Icons/icons.hpp>

#include <qcustomplot.h>

#endif // SCIQLOPPLOTS_BINDINGS_H

#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"

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

void SciQLopMultiGraphBase::clear_graphs(bool) { /* filled in Task 2 */ }
void SciQLopMultiGraphBase::build_data_source(const PyBuffer&, const PyBuffer&) { /* Task 2 */ }
void SciQLopMultiGraphBase::sync_components() { /* Task 2 */ }
void SciQLopMultiGraphBase::set_data(PyBuffer, PyBuffer) { /* Task 2 */ }
QList<PyBuffer> SciQLopMultiGraphBase::data() const noexcept { return {}; }
void SciQLopMultiGraphBase::set_x_axis(SciQLopPlotAxisInterface*) noexcept { /* Task 2 */ }
void SciQLopMultiGraphBase::set_y_axis(SciQLopPlotAxisInterface*) noexcept { /* Task 2 */ }
void SciQLopMultiGraphBase::create_graphs(const QStringList&) { /* Task 2 */ }

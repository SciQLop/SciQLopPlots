/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopTheme.hpp"
#include "SciQLopPlots/SciQLopTimeSeriesPlot.hpp"
#include "SciQLopPlots/unique_names_factory.hpp"

#include "SciQLopPlots/DragNDrop/PlaceHolderManager.hpp"
#include "SciQLopPlots/MultiPlots/CrosshairSynchronizer.hpp"

#include "SciQLopPlots/Inspector/InspectorExtensionHolder.hpp"
#include "SciQLopPlots/Inspector/Model/Model.hpp"

#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp"
#include "SciQLopPlots/constants.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp"
#include "SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp"
#include "SciQLopPlots/MultiPlots/TimeAxisSynchronizer.hpp"
#include "SciQLopPlots/MultiPlots/VPlotsAlign.hpp"
#include "SciQLopPlots/MultiPlots/XAxisSynchronizer.hpp"

#include <cpp_utils/containers/algorithms.hpp>

#include <QFileInfo>
#include <QKeyEvent>
#include <QPainter>
#include <QPdfWriter>

SciQLopMultiPlotPanel::SciQLopMultiPlotPanel(QWidget* parent, bool synchronize_x,
                                             bool synchronize_time, Qt::Orientation orientation)
        : SciQLopPlotPanelInterface { parent }, _uuid { QUuid::createUuid() }
        , m_extension_holder { std::make_unique<InspectorExtensionHolder>(
              this, [this]() { Q_EMIT inspector_extensions_changed(); }) }
{
    _container = new SciQLopPlotContainer(this, orientation);
    connect(_container, &SciQLopPlotContainer::plot_list_changed, this,
            &SciQLopMultiPlotPanel::plot_list_changed);
    connect(_container, &SciQLopPlotContainer::plot_added, this,
            &SciQLopMultiPlotPanel::plot_added);
    connect(_container, &SciQLopPlotContainer::plot_removed, this,
            &SciQLopMultiPlotPanel::plot_removed);
    connect(_container, &SciQLopPlotContainer::plot_inserted, this,
            &SciQLopMultiPlotPanel::plot_inserted);
    connect(_container, &SciQLopPlotContainer::plot_moved, this,
            &SciQLopMultiPlotPanel::plot_moved);

    setWidget(_container);
    this->setWidgetResizable(true);

    // Vertical layout is the default layout where plots are aligned vertically
    // and drag&drop is enabled
    if (orientation == Qt::Vertical)
    {
        ::register_behavior<VPlotsAlign>(_container);
        _place_holder_manager = new PlaceHolderManager(this);
    }
    if (synchronize_x)
        ::register_behavior<XAxisSynchronizer>(_container);
    if (synchronize_time)
    {
        auto b = ::register_behavior<TimeAxisSynchronizer>(_container);
        _default_plot_type = PlotType::TimeSeries;
        connect(b, &TimeAxisSynchronizer::range_changed, this,
                [this](const SciQLopPlotRange& range)
                {
                    this->_container->set_time_axis_range(range);
                    emit this->time_range_changed(range);
                });
    }
    if (synchronize_x)
        ::register_behavior<CrosshairSynchronizer>(_container, AxisType::XAxis);
    else if (synchronize_time)
        ::register_behavior<CrosshairSynchronizer>(_container, AxisType::TimeAxis);
    this->setAcceptDrops(true);
    setObjectName(UniqueNamesFactory::unique_name("Panel"));

    if (qobject_cast<SciQLopPlotPanelInterface*>(parent) == nullptr)
        PlotsModel::instance()->addTopLevelNode(this);
}

SciQLopMultiPlotPanel::~SciQLopMultiPlotPanel()
{

}

void SciQLopMultiPlotPanel::replot(bool immediate)
{
    _container->replot(immediate);
}

void SciQLopMultiPlotPanel::add_panel(SciQLopPlotPanelInterface* panel)
{
    _container->addWidget(panel);
    Q_EMIT panel_added(panel);
}

void SciQLopMultiPlotPanel::insert_panel(int index, SciQLopPlotPanelInterface* panel)
{
    _container->insertWidget(index, panel);
    Q_EMIT panel_added(panel);
}

void SciQLopMultiPlotPanel::remove_panel(SciQLopPlotPanelInterface* panel)
{
    _container->removeWidget(panel, true);
    Q_EMIT panel_removed(panel);
}

void SciQLopMultiPlotPanel::move_panel(int from, int to) { }

void SciQLopMultiPlotPanel::add_plot(SciQLopPlotInterface* plot)
{
    _container->add_plot(plot);
}

void SciQLopMultiPlotPanel::remove_plot(SciQLopPlotInterface* plot)
{
    _container->remove_plot(plot);
}

SciQLopPlotInterface* SciQLopMultiPlotPanel::plot_at(int index) const
{
    return _container->plot_at(index);
}

QWidget *SciQLopMultiPlotPanel::widget_at(const QPointF &pos) const
{
    return _container->widget_at(_container->mapFromParent(pos));
}

QList<QPointer<SciQLopPlotInterface>> SciQLopMultiPlotPanel::plots() const
{
    return _container->plots();
}

QList<QWidget *> SciQLopMultiPlotPanel::child_widgets() const
{
    return _container->child_widgets();
}

void SciQLopMultiPlotPanel::insert_plot(int index, SciQLopPlotInterface* plot)
{
    _container->insert_plot(index, plot);
}

void SciQLopMultiPlotPanel::move_plot(int from, int to)
{
    _container->move_plot(from, to);
}

void SciQLopMultiPlotPanel::move_plot(SciQLopPlotInterface* plot, int to)
{
    _container->move_plot(plot, to);
}

void SciQLopMultiPlotPanel::clear()
{
    _container->clear();
}

int SciQLopMultiPlotPanel::index(SciQLopPlotInterface* plot) const
{
    return _container->index(plot);
}

int SciQLopMultiPlotPanel::index(const QPointF& pos) const
{
    return _container->index(_container->mapFromParent(pos));
}

int SciQLopMultiPlotPanel::index_from_global_position(const QPointF& pos) const
{
    return _container->index(_container->mapFromGlobal(pos));
}

int SciQLopMultiPlotPanel::indexOf(QWidget* widget) const
{
    return _container->indexOf(widget);
}

bool SciQLopMultiPlotPanel::contains(SciQLopPlotInterface* plot) const
{
    return _container->contains(plot);
}

bool SciQLopMultiPlotPanel::empty() const
{
    return _container->empty();
}

std::size_t SciQLopMultiPlotPanel::size() const
{
    return _container->size();
}

std::size_t SciQLopMultiPlotPanel::content_height() const
{
    return _container->content_height();
}

std::size_t SciQLopMultiPlotPanel::content_width() const
{
    return _container->content_width();
}

void SciQLopMultiPlotPanel::addWidget(QWidget* widget)
{
    _container->addWidget(widget);
}

void SciQLopMultiPlotPanel::removeWidget(QWidget* widget)
{
    _container->removeWidget(widget, true);
}

SciQLopPlotInterface* SciQLopMultiPlotPanel::create_plot(int index, PlotType plot_type)
{
    SciQLopPlotInterface* plot = nullptr;
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            plot = new SciQLopPlot();
            break;
        case ::PlotType::TimeSeries:
            plot = new SciQLopTimeSeriesPlot();
            break;
        default:
            break;
    }
    if (index == -1)
        add_plot(plot);
    else
        insert_plot(index, plot);
    return plot;
}

void SciQLopMultiPlotPanel::set_x_axis_range(const SciQLopPlotRange& range)
{
    _container->set_x_axis_range(range);
}

const SciQLopPlotRange& SciQLopMultiPlotPanel::x_axis_range() const
{
    return _container->x_axis_range();
}

void SciQLopMultiPlotPanel::set_time_axis_range(const SciQLopPlotRange& range)
{
    if (_container->empty())
        if (auto* time_axis_synchronizer = ::behavior<TimeAxisSynchronizer>(_container))
            time_axis_synchronizer->set_axis_range(range);
    _container->set_time_axis_range(range);
}

const SciQLopPlotRange& SciQLopMultiPlotPanel::time_axis_range() const
{
    return _container->time_axis_range();
}

SciQLopPlotCollectionBehavior*
SciQLopMultiPlotPanel::register_behavior(SciQLopPlotCollectionBehavior* behavior)
{
    return _container->register_behavior(behavior);
}

SciQLopPlotCollectionBehavior* SciQLopMultiPlotPanel::behavior(const QString& type_name) const
{
    return _container->behavior(type_name);
}

void SciQLopMultiPlotPanel::remove_behavior(const QString& type_name)
{
    _container->remove_behavior(type_name);
}

void SciQLopMultiPlotPanel::add_accepted_mime_type(PlotDragNDropCallback* callback)
{
    if (!callback)
        return;
    callback->setParent(this);
    _accepted_mime_types[callback->mime_type()] = callback;
}

void SciQLopMultiPlotPanel::setSelected(bool selected)
{
    _selected = selected;
    emit selectionChanged(selected);
}

QList<QColor> SciQLopMultiPlotPanel::color_palette() const noexcept
{
    return _container->color_palette();
}

void SciQLopMultiPlotPanel::set_color_palette(const QList<QColor>& palette) noexcept
{
    _container->set_color_palette(palette);
}

QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
SciQLopMultiPlotPanel::plot_impl(const PyBuffer& x, const PyBuffer& y, QStringList labels,
                                 QList<QColor> colors, PlotType plot_type, GraphType graph_type,
                                 GraphMarkerShape marker, int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopGraphInterface>(index, graph_type, x, y, labels,
                                                             colors, marker, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopGraphInterface>(index, graph_type, x, y,
                                                                       labels, colors, marker, metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
SciQLopMultiPlotPanel::plot_impl(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z,
                                 QString name, bool y_log_scale, bool z_log_scale,
                                 PlotType plot_type, int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopColorMapInterface>(index, GraphType::ColorMap, x, y, z,
                                                                name, y_log_scale, z_log_scale,metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopColorMapInterface>(
                index, GraphType::ColorMap, x, y, z, name, y_log_scale, z_log_scale,metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>
SciQLopMultiPlotPanel::plot_impl(const QList<PyBuffer>& values, QStringList labels,
                                 QList<QColor> colors, GraphMarkerShape marker, int index, QVariantMap metaData)
{
    auto* plot = new SciQLopNDProjectionPlot();
    if (index == -1)
        add_plot(plot);
    else
        insert_plot(index, plot);

    return { plot, plot->parametric_curve(values, labels, colors,marker, metaData) };
}

QPair<SciQLopPlotInterface*, SciQLopGraphInterface*> SciQLopMultiPlotPanel::plot_impl(
    GetDataPyCallable callable, QStringList labels, QList<QColor> colors, GraphType graph_type,
    GraphMarkerShape marker, PlotType plot_type, QObject* sync_with, int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopGraphInterface>(index, graph_type, callable, labels,
                                                             colors, marker, sync_with, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopGraphInterface>(
                index, graph_type, callable, labels, colors, marker, sync_with, metaData);
            break;
        case ::PlotType::Projections:
            return _plot<SciQLopNDProjectionPlot, SciQLopGraphInterface>(
                index, graph_type, callable, labels, colors, marker, sync_with, metaData);
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
SciQLopMultiPlotPanel::plot_impl(GetDataPyCallable callable, QString name, bool y_log_scale,
                                 bool z_log_scale, PlotType plot_type, QObject* sync_with,
                                 int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot<SciQLopPlot, SciQLopColorMapInterface>(
                index, GraphType::ColorMap, callable, name, y_log_scale, z_log_scale, sync_with, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot<SciQLopTimeSeriesPlot, SciQLopColorMapInterface>(
                index, GraphType::ColorMap, callable, name, y_log_scale, z_log_scale, sync_with, metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
SciQLopMultiPlotPanel::plot_impl(const PyBuffer& x, const PyBuffer& y, QString name, int key_bins,
                                 int value_bins, PlotType plot_type, int index,
                                 QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot_histogram2d<SciQLopPlot>(
                index, x, y, name, key_bins, value_bins, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot_histogram2d<SciQLopTimeSeriesPlot>(
                index, x, y, name, key_bins, value_bins, metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

QPair<SciQLopPlotInterface*, SciQLopColorMapInterface*>
SciQLopMultiPlotPanel::plot_impl(GetDataPyCallable callable, QString name, int key_bins,
                                 int value_bins, PlotType plot_type, QObject* sync_with,
                                 int index, QVariantMap metaData)
{
    switch (plot_type)
    {
        case ::PlotType::BasicXY:
            return _plot_histogram2d<SciQLopPlot>(
                index, std::move(callable), name, key_bins, value_bins, sync_with, metaData);
            break;
        case ::PlotType::TimeSeries:
            return _plot_histogram2d<SciQLopTimeSeriesPlot>(
                index, std::move(callable), name, key_bins, value_bins, sync_with, metaData);
            break;
        default:
            break;
    }
    return { nullptr, nullptr };
}

void SciQLopMultiPlotPanel::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_O:
            _container->organize_plots();
            event->accept();
            break;
        default:
            break;
    }
    if (!event->isAccepted())
        QScrollArea::keyPressEvent(event);
}

void SciQLopMultiPlotPanel::dragEnterEvent(QDragEnterEvent* event)
{
    using namespace cpp_utils;
    const auto formats = event->mimeData()->formats();
    for (const auto& format : formats)
    {
        if (containers::contains(_accepted_mime_types, format))
        {
            event->acceptProposedAction();
            _current_callback = _accepted_mime_types[format];
            if (_current_callback->create_placeholder())
                _place_holder_manager->dragEnterEvent(event);
            return;
        }
    }
    _current_callback = nullptr;
}

void SciQLopMultiPlotPanel::dragMoveEvent(QDragMoveEvent* event)
{
    if (_current_callback)
    {
        if (_current_callback->create_placeholder())
            _place_holder_manager->dragMoveEvent(event);
        event->acceptProposedAction();
    }
}

void SciQLopMultiPlotPanel::dragLeaveEvent(QDragLeaveEvent* event)
{
    if (_current_callback && _current_callback->create_placeholder())
        _place_holder_manager->dragLeaveEvent(event);
    _current_callback = nullptr;
}

void SciQLopMultiPlotPanel::dropEvent(QDropEvent* event)
{
    if (_current_callback)
    {
        SciQLopPlotInterface* drop_plot = nullptr;
        auto drop_result = _place_holder_manager->dropEvent(event);
        if (drop_result.location == DropLocation::NewPlot)
        {
            drop_plot = create_plot(drop_result.index, _default_plot_type);
        }
        else
        {
            drop_plot = qobject_cast<SciQLopPlotInterface*>(
                widget_at(event->position()));
        }

        event->acceptProposedAction();
        if (drop_plot)
            _current_callback->call(drop_plot, event->mimeData());
        _current_callback = nullptr;
    }
}

bool SciQLopMultiPlotPanel::save_pdf(const QString& filename, int width, int height)
{
    auto plot_list = _container->plots();
    if (plot_list.isEmpty())
        return false;

    const int totalW = (width > 0) ? width : _container->width();
    const int totalH = (height > 0) ? height : _container->height();

    const int containerH = _container->height();
    if (totalW <= 0 || totalH <= 0 || containerH <= 0)
        return false;

    QPdfWriter writer(filename);
    writer.setPageSize(QPageSize(QSizeF(totalW, totalH), QPageSize::Point));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(72);

    QCPPainter painter;
    painter.begin(&writer);
    if (!painter.isActive())
        return false;

    painter.setMode(QCPPainter::pmVectorized);
    painter.setMode(QCPPainter::pmNoCaching);

    int yOffset = 0;
    for (auto& plotPtr : plot_list)
    {
        if (plotPtr.isNull())
            continue;
        auto* sciPlot = qobject_cast<SciQLopPlot*>(plotPtr.data());
        if (!sciPlot)
            continue;

        auto* qcpPlot = sciPlot->qcp_plot();
        if (!qcpPlot)
            continue;

        const int plotH = static_cast<int>(
            static_cast<double>(plotPtr->height()) / containerH * totalH);
        const int plotW = totalW;

        painter.save();
        painter.translate(0, yOffset);
        qcpPlot->toPainter(&painter, plotW, plotH);
        painter.restore();
        yOffset += plotH;
    }

    painter.end();
    return true;
}

static bool _save_panel_raster(QWidget* container, const QString& filename,
                               const char* format, int width, int height,
                               double scale, int quality)
{
    if (!container)
        return false;

    const auto fullSize = container->sizeHint().expandedTo(container->size());
    int targetW = (width > 0) ? width : static_cast<int>(fullSize.width() * scale);
    int targetH = (height > 0) ? height : static_cast<int>(fullSize.height() * scale);

    QPixmap pixmap(targetW, targetH);
    pixmap.fill(Qt::transparent);
    container->render(&pixmap, QPoint(), QRegion(), QWidget::DrawChildren);

    if (pixmap.isNull())
        return false;

    return pixmap.save(filename, format, quality);
}

bool SciQLopMultiPlotPanel::save_png(const QString& filename, int width, int height,
                                     double scale, int quality)
{
    return _save_panel_raster(_container, filename, "PNG", width, height, scale, quality);
}

bool SciQLopMultiPlotPanel::save_jpg(const QString& filename, int width, int height,
                                     double scale, int quality)
{
    return _save_panel_raster(_container, filename, "JPG", width, height, scale, quality);
}

bool SciQLopMultiPlotPanel::save_bmp(const QString& filename, int width, int height,
                                     double scale)
{
    return _save_panel_raster(_container, filename, "BMP", width, height, scale, -1);
}

bool SciQLopMultiPlotPanel::save(const QString& filename, int width, int height,
                                 double scale, int quality)
{
    auto ext = QFileInfo(filename).suffix().toLower();
    if (ext == "pdf")
        return save_pdf(filename, width, height);
    if (ext == "png")
        return save_png(filename, width, height, scale, quality);
    if (ext == "jpg" || ext == "jpeg")
        return save_jpg(filename, width, height, scale, quality);
    if (ext == "bmp")
        return save_bmp(filename, width, height, scale);
    return false;
}

void SciQLopMultiPlotPanel::_install_span_creator(SciQLopPlot* plot)
{
    auto* qcp = plot->qcp_plot();

    qcp->setItemCreator(
        [this, qcp](QCustomPlot*, QCPAxis*, QCPAxis*) -> QCPAbstractItem*
        {
            auto* vspan = new QCPItemVSpan(qcp);
            vspan->setPen(QPen(m_span_creation_color.darker(120)));
            vspan->setBrush(QBrush(m_span_creation_color));
            vspan->setLayer(Constants::LayersNames::Spans);

            _clear_preview_spans();
            for (auto& p : plots())
            {
                auto* sp = dynamic_cast<SciQLopPlot*>(p.data());
                if (!sp)
                    continue;
                auto* other_qcp = sp->qcp_plot();
                if (other_qcp == qcp)
                    continue;
                auto* preview = new QCPItemVSpan(other_qcp);
                preview->setPen(QPen(m_span_creation_color.darker(120)));
                preview->setBrush(QBrush(m_span_creation_color));
                preview->setLayer(Constants::LayersNames::Spans);
                m_creation_state.preview_spans.append(preview);
                other_qcp->replot(QCustomPlot::rpQueuedReplot);
            }
            return vspan;
        });

    qcp->setItemPositioner(
        [this](QCPAbstractItem* item, double anchorKey, double, double currentKey, double)
        {
            double lower = std::min(anchorKey, currentKey);
            double upper = std::max(anchorKey, currentKey);
            if (auto* vspan = qobject_cast<QCPItemVSpan*>(item))
                vspan->setRange(QCPRange(lower, upper));
            QSet<QCustomPlot*> to_replot;
            for (auto& preview : m_creation_state.preview_spans)
            {
                if (preview)
                {
                    preview->setRange(QCPRange(lower, upper));
                    to_replot.insert(preview->parentPlot());
                }
            }
            for (auto* p : to_replot)
                p->replot(QCustomPlot::rpQueuedReplot);
        });

    auto& conns = m_per_plot_connections[qcp];
    conns.append(connect(qcp, &QCustomPlot::itemCreated, this,
        [this, qcp](QCPAbstractItem* item) { _on_item_created(qcp, item); }));
    conns.append(connect(qcp, &QCustomPlot::itemCanceled, this,
        [this, qcp]() { _on_item_canceled(qcp); }));

    qcp->setCreationModifier(m_span_creation_modifier);
}

void SciQLopMultiPlotPanel::_uninstall_span_creator(SciQLopPlot* plot)
{
    auto* qcp = plot->qcp_plot();
    qcp->setItemCreator(nullptr);
    qcp->setItemPositioner(nullptr);
    if (auto it = m_per_plot_connections.find(qcp); it != m_per_plot_connections.end())
    {
        for (auto& conn : it->second)
            disconnect(conn);
        m_per_plot_connections.erase(it);
    }
}

void SciQLopMultiPlotPanel::_clear_preview_spans()
{
    QSet<QCustomPlot*> to_replot;
    for (auto& preview : m_creation_state.preview_spans)
    {
        if (preview)
        {
            auto* plot = preview->parentPlot();
            plot->removeItem(preview);
            to_replot.insert(plot);
        }
    }
    for (auto* p : to_replot)
        p->replot(QCustomPlot::rpQueuedReplot);
    m_creation_state.clear();
}

void SciQLopMultiPlotPanel::_on_item_created(QCustomPlot* qcp, QCPAbstractItem* item)
{
    auto* vspan = qobject_cast<QCPItemVSpan*>(item);
    if (!vspan)
        return;

    auto qcp_range = vspan->range();
    SciQLopPlotRange range(qcp_range.lower, qcp_range.upper);

    qcp->removeItem(vspan);
    _clear_preview_spans();

    auto* mpvspan
        = new MultiPlotsVerticalSpan(this, range, m_span_creation_color, false, true, "");
    Q_EMIT span_created(mpvspan);
}

void SciQLopMultiPlotPanel::_on_item_canceled(QCustomPlot*)
{
    _clear_preview_spans();
    Q_EMIT span_creation_canceled();
}

void SciQLopMultiPlotPanel::set_theme(SciQLopTheme* theme)
{
    if (m_theme_connection)
    {
        disconnect(m_theme_connection);
        m_theme_connection = {};
    }

    m_theme = theme;

    for (auto& p : plots())
    {
        if (p)
            p->set_theme(theme);
    }

    if (theme)
    {
        m_theme_connection = connect(this, &SciQLopMultiPlotPanel::plot_added, this,
            [this](SciQLopPlotInterface* plot)
            {
                if (m_theme && plot)
                    plot->set_theme(m_theme);
            });
    }
}

void SciQLopMultiPlotPanel::set_span_creation_enabled(bool enabled)
{
    if (m_span_creation_enabled == enabled)
        return;
    m_span_creation_enabled = enabled;

    if (enabled)
    {
        for (auto& p : plots())
        {
            if (auto* sp = dynamic_cast<SciQLopPlot*>(p.data()))
                _install_span_creator(sp);
        }
        m_creation_connections.append(
            connect(this, &SciQLopMultiPlotPanel::plot_added, this,
                [this](SciQLopPlotInterface* plot)
                {
                    if (m_span_creation_enabled)
                    {
                        if (auto* sp = dynamic_cast<SciQLopPlot*>(plot))
                            _install_span_creator(sp);
                    }
                }));
        m_creation_connections.append(
            connect(this, &SciQLopMultiPlotPanel::plot_removed, this,
                [this](SciQLopPlotInterface* plot)
                {
                    if (auto* sp = dynamic_cast<SciQLopPlot*>(plot))
                    {
                        _clear_preview_spans();
                        _uninstall_span_creator(sp);
                    }
                }));
    }
    else
    {
        _clear_preview_spans();
        for (auto& conn : m_creation_connections)
            disconnect(conn);
        m_creation_connections.clear();
        for (auto& p : plots())
        {
            if (auto* sp = dynamic_cast<SciQLopPlot*>(p.data()))
                _uninstall_span_creator(sp);
        }
    }
}

void SciQLopMultiPlotPanel::set_span_creation_modifier(Qt::KeyboardModifier modifier)
{
    m_span_creation_modifier = modifier;
    if (m_span_creation_enabled)
    {
        for (auto& p : plots())
        {
            if (auto* sp = dynamic_cast<SciQLopPlot*>(p.data()))
                sp->qcp_plot()->setCreationModifier(modifier);
        }
    }
}

void SciQLopMultiPlotPanel::add_inspector_extension(InspectorExtension* extension)
{
    if (m_extension_holder->add(extension))
        Q_EMIT inspector_extension_added(extension);
}

void SciQLopMultiPlotPanel::remove_inspector_extension(InspectorExtension* extension)
{
    if (m_extension_holder->remove(extension))
        Q_EMIT inspector_extension_removed(extension);
}

QList<InspectorExtension*> SciQLopMultiPlotPanel::inspector_extensions() const
{
    return m_extension_holder->list();
}

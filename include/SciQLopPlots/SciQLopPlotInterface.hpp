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
#pragma once

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlotLegendInterface.hpp"
#include "SciQLopPlots/SciQLopPlotGroup.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"
#include "SciQLopPlots/enums.hpp"
#include "SciQLopPlots/unique_names_factory.hpp"
#include <QFrame>
#include <QPointF>
#include <QUuid>
#include <memory>
#include <optional>
#include <qcustomplot.h>

class QPixmap;
class SciQLopTextItem;
class SciQLopPixmapItem;
class SciQLopEllipseItem;
class SciQLopCurvedLineItem;
class SciQLopTheme;
class SciQLopItemInterface;
class SciQLopStraightLine;
class SciQLopSpan;
class InspectorExtension;
class SciQLopHistogram;
#include "SciQLopPlots/Inspector/InspectorExtensionHolder.hpp"

namespace SciQLopPlots
{
//! Marks \a part as built by a panel-wide object: removing it alone would leave that object out of step.
inline void mark_panel_part(QObject* part) { part->setProperty("_sciqlop_panel_part", true); }
inline bool is_panel_part(const QObject* part) { return part->property("_sciqlop_panel_part").toBool(); }
}

class SciQLopPlotInterface : public QFrame
{

    Q_OBJECT

    QList<AxisType> m_unsynchronized_axes;
    void check_removable(const QObject* x) const;

protected:
    QList<SciQLopPlotAxisInterface*> m_axes_to_rescale;
    QList<SciQLopPlotAxisInterface*> m_frozen_axes;
    QUuid m_uuid;
    bool m_selected = false;
    std::unique_ptr<InspectorExtensionHolder> m_extension_holder;

    inline virtual SciQLopGraphInterface*
    plot_impl(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = GraphType::Line,
              ::GraphMarkerShape marker = GraphMarkerShape::NoMarker, QVariantMap metaData={})
    {
        throw_unsupported(factory_name(graph_type), *this);
    }

    inline virtual SciQLopGraphInterface*
    plot_impl(const QList<SciQLopPyBuffer>& values, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = GraphType::Line,
              ::GraphMarkerShape marker = GraphMarkerShape::NoMarker, QVariantMap metaData={})
    {
        throw_unsupported("projection", *this);
    }

    inline virtual SciQLopColorMapInterface* plot_impl(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                                                       const SciQLopPyBuffer& z,
                                                       QString name = "ColorMap",
                                                       bool y_log_scale = false,
                                                       bool z_log_scale = false, QVariantMap metaData={})
    {
        throw_unsupported("colormap", *this);
    }

    inline virtual SciQLopGraphInterface*
    plot_impl(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = GraphType::Line,
              ::GraphMarkerShape marker = GraphMarkerShape::NoMarker,
              QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        throw_unsupported(factory_name(graph_type), *this);
    }

    inline virtual SciQLopColorMapInterface*
    plot_impl(GetDataPyCallable callable, QString name = "ColorMap",
              bool y_log_scale = false, bool z_log_scale = false, QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        throw_unsupported("colormap", *this);
    }

    inline virtual SciQLopColorMapInterface*
    plot_impl(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
              QString name = "Histogram2D", int x_bins = 100,
              int y_bins = 100, bool x_bins_log = false, bool y_bins_log = false,
              QVariantMap metaData = {})
    {
        throw_unsupported("histogram2d", *this);
    }

    inline virtual SciQLopColorMapInterface*
    plot_impl(GetDataPyCallable callable, QString name = "Histogram2D",
              int x_bins = 100, int y_bins = 100, bool x_bins_log = false,
              bool y_bins_log = false, QObject* sync_with = nullptr,
              QVariantMap metaData = {})
    {
        throw_unsupported("histogram2d", *this);
    }

public:
    Q_PROPERTY(bool selected READ selected WRITE set_selected NOTIFY selection_changed FINAL)

    SciQLopPlotInterface(QWidget* parent = nullptr, const QString& name = "")
            : QFrame(parent), m_uuid(QUuid::createUuid())
            , m_extension_holder { std::make_unique<InspectorExtensionHolder>(
                  this, [this]() { Q_EMIT inspector_extensions_changed(); }) }
    {
        if (!name.isEmpty())
            setObjectName(name);
        else
            setObjectName(UniqueNamesFactory::unique_name("Plot"));
        connect(this, &QObject::objectNameChanged, this, &SciQLopPlotInterface::name_changed);
    }

    virtual ~SciQLopPlotInterface() = default;

    inline QString name() const { return objectName(); }
    inline void set_name(const QString& name) { setObjectName(name); }

    inline QUuid uuid() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return m_uuid;
    }

    inline virtual SciQLopPlotAxisInterface* time_axis() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* x_axis() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* y_axis() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* z_axis() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* y2_axis() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* x2_axis() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* axis(AxisType axis) const noexcept
    {
        switch (axis)
        {
            case AxisType::TimeAxis:
                return time_axis();
            case AxisType::XAxis:
                return x_axis();
            case AxisType::YAxis:
                return y_axis();
            case AxisType::ZAxis:
                return z_axis();
            case AxisType::XAxis2:
                return x2_axis();
            case AxisType::YAxis2:
                return y2_axis();
            default:
                return nullptr;
        }
    }

    //! A panel's synchronisers leave an unsynchronized axis alone (a histogram's x holds
    //! values, not the x the panel shares). They re-wire the plot when this changes.
    inline void set_axis_synchronized(AxisType axis, bool synchronized) noexcept
    {
        if (axis_synchronized(axis) == synchronized)
            return;
        m_unsynchronized_axes.removeAll(axis);
        if (!synchronized)
            m_unsynchronized_axes.append(axis);
        Q_EMIT axis_synchronized_changed(axis, synchronized);
    }

    inline bool axis_synchronized(AxisType axis) const noexcept
    {
        return !m_unsynchronized_axes.contains(axis);
    }

    //! The axis a panel synchroniser drives, nullptr if this plot opted out of it.
    inline SciQLopPlotAxisInterface* synchronized_axis(AxisType axis) const noexcept
    {
        return axis_synchronized(axis) ? this->axis(axis) : nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* axis(Qt::AnchorPoint pos, int index = 0) const noexcept
    {
        if (index == 0)
        {

            switch (pos)
            {
                case Qt::AnchorPoint::AnchorBottom:
                    return x_axis();
                case Qt::AnchorPoint::AnchorLeft:
                    return y_axis();
                case Qt::AnchorPoint::AnchorRight:
                    return y2_axis();
                case Qt::AnchorPoint::AnchorTop:
                    return x2_axis();
                default:
                    break;
            }
        }
        return nullptr;
    }

    inline virtual SciQLopPlotAxisInterface* axis(const QString& name) const noexcept
    {
        if (auto ax = x_axis(); ax && ax->objectName() == name)
            return ax;
        if (auto ax = y_axis(); ax && ax->objectName() == name)
            return ax;
        if (auto ax = z_axis(); ax && ax->objectName() == name)
            return ax;
        if (auto ax = x2_axis(); ax && ax->objectName() == name)
            return ax;
        if (auto ax = y2_axis(); ax && ax->objectName() == name)
            return ax;
        return nullptr;
    }

    //! None for plots without one (a widget in a panel): an answer, not an unsupported call.
    inline virtual SciQLopPlotLegendInterface* legend() const noexcept { return nullptr; }

    inline virtual void set_theme(SciQLopTheme* theme) { Q_UNUSED(theme); }
    inline virtual SciQLopTheme* theme() const { return nullptr; }

    inline virtual void set_scroll_factor(double factor) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual double scroll_factor() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return std::nan("");
    }

    inline virtual void enable_cursor(bool enable = true) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void minimize_margins() { WARN_ABSTRACT_METHOD; }

    inline virtual void replot(bool immediate = false) { WARN_ABSTRACT_METHOD; }

    inline virtual void rescale_axes() noexcept { rescale_axes(m_axes_to_rescale); }

    inline virtual void rescale_axes(const QList<SciQLopPlotAxisInterface*>& axes) noexcept
    {
        for (auto ax : axes)
        {
            ax->rescale();
        }
    }

    //! Rescales the axis under the cursor, else the selected axes, else all
    //! rescalable axes (minus frozen ones). Formerly the hard-coded 'M' shortcut;
    //! now a plain method so a host application can bind its own key to it.
    void rescale_hovered_or_selected_axes() noexcept
    {
        QList<SciQLopPlotAxisInterface*> axes;
        QList<SciQLopPlotAxisInterface*> axes_to_rescale;
        if (auto ax = axis_at(mapFromGlobal(QCursor::pos())))
            axes.append(ax);
        else
            axes = selected_axes();
        if (axes.isEmpty())
            axes_to_rescale = m_axes_to_rescale;
        else
            axes_to_rescale = axes;

        for (auto ax : m_frozen_axes)
            axes_to_rescale.removeAll(ax);
        rescale_axes(axes_to_rescale);
    }

    //! Toggles log scale on the axis under the cursor, else the selected axes.
    //! Formerly the hard-coded 'L' shortcut; see rescale_hovered_or_selected_axes().
    void toggle_log_scale_hovered_or_selected_axes() noexcept
    {
        QList<SciQLopPlotAxisInterface*> axes;
        if (auto ax = axis_at(mapFromGlobal(QCursor::pos())))
            axes.append(ax);
        else
            axes = selected_axes();
        for (auto ax : axes)
            ax->set_log(not ax->log());
    }

    inline virtual SciQLopGraphInterface*
    line(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, QStringList labels = QStringList(),
         QList<QColor> colors = QList<QColor>(),
         ::GraphMarkerShape marker = GraphMarkerShape::NoMarker, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(x, y, labels, colors, ::GraphType::Line, marker, metaData), name);
    }

    inline virtual SciQLopGraphInterface*
    scatter(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, QStringList labels = QStringList(),
            QList<QColor> colors = QList<QColor>(),
            ::GraphMarkerShape marker = GraphMarkerShape::Cross, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(x, y, labels, colors, ::GraphType::Scatter, marker, metaData), name);
    }

    inline virtual SciQLopGraphInterface*
    parametric_curve(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::GraphMarkerShape marker = GraphMarkerShape::NoMarker, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(x, y, labels, colors, ::GraphType::ParametricCurve, marker, metaData), name);
    }

    inline virtual SciQLopGraphInterface*
    parametric_curve(const QList<SciQLopPyBuffer>& values, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::GraphMarkerShape marker = GraphMarkerShape::NoMarker, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(values, labels, colors, ::GraphType::ParametricCurve, marker,metaData), name);
    }

    inline virtual SciQLopColorMapInterface* colormap(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                                                      const SciQLopPyBuffer& z,
                                                      QString name = "ColorMap",
                                                      bool y_log_scale = false,
                                                      bool z_log_scale = false, QVariantMap metaData={})
    {
        return plot_impl(x, y, z, name, y_log_scale, z_log_scale,metaData);
    }

    inline virtual SciQLopGraphInterface*
    line(GetDataPyCallable callable, QStringList labels = QStringList(),
         QList<QColor> colors = QList<QColor>(),
         ::GraphMarkerShape marker = GraphMarkerShape::NoMarker, QObject* sync_with = nullptr, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(callable, labels, colors, ::GraphType::Line, marker, sync_with,metaData), name);
    }

    inline virtual SciQLopGraphInterface*
    scatter(GetDataPyCallable callable, QStringList labels = QStringList(),
            QList<QColor> colors = QList<QColor>(),
            GraphMarkerShape marker = GraphMarkerShape::Cross, QObject* sync_with = nullptr, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(callable, labels, colors, ::GraphType::Scatter, marker, sync_with,metaData), name);
    }

    inline virtual SciQLopGraphInterface*
    parametric_curve(GetDataPyCallable callable, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::GraphMarkerShape marker = GraphMarkerShape::NoMarker,
                     QObject* sync_with = nullptr, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(callable, labels, colors, ::GraphType::ParametricCurve, marker, sync_with,metaData), name);
    }

    inline virtual SciQLopColorMapInterface*
    colormap(GetDataPyCallable callable, QString name = "ColorMap",
             bool y_log_scale = false, bool z_log_scale = false, QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        return plot_impl(callable, name, y_log_scale, z_log_scale, sync_with,metaData);
    }

    inline virtual SciQLopGraphInterface*
    waterfall(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(),
              ::GraphMarkerShape marker = GraphMarkerShape::NoMarker,
              QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(x, y, labels, colors, ::GraphType::Waterfall, marker, metaData), name);
    }

    inline virtual SciQLopGraphInterface*
    waterfall(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(),
              ::GraphMarkerShape marker = GraphMarkerShape::NoMarker,
              QObject* sync_with = nullptr, QVariantMap metaData = {}, QString name = QString())
    {
        return with_name(plot_impl(callable, labels, colors, ::GraphType::Waterfall, marker, sync_with,
                         metaData), name);
    }

    inline virtual SciQLopColorMapInterface*
    histogram2d(const SciQLopPyBuffer& x, const SciQLopPyBuffer& y,
                QString name = "Histogram2D", int x_bins = 100,
                int y_bins = 100, bool x_bins_log = false, bool y_bins_log = false,
                QVariantMap metaData = {})
    {
        return plot_impl(x, y, name, x_bins, y_bins, x_bins_log, y_bins_log, metaData);
    }

    inline virtual SciQLopColorMapInterface*
    histogram2d(GetDataPyCallable callable, QString name = "Histogram2D",
                int x_bins = 100, int y_bins = 100, bool x_bins_log = false,
                bool y_bins_log = false, QObject* sync_with = nullptr,
                QVariantMap metaData = {})
    {
        return plot_impl(callable, name, x_bins, y_bins, x_bins_log, y_bins_log,
                         sync_with, metaData);
    }

    inline virtual SciQLopHistogram* histogram(const SciQLopPyBuffer& values,
                                               QString name = "Histogram",
                                               int bins = 64, QVariantMap metadata = {})
    {
        report_unsupported_call("histogram", typeid(*this).name());
        return nullptr;
    }

    inline virtual SciQLopHistogram* histogram(GetDataPyCallable callable,
                                               QString name = "Histogram",
                                               int bins = 64, QObject* sync_with = nullptr,
                                               QVariantMap metadata = {})
    {
        report_unsupported_call("histogram", typeid(*this).name());
        return nullptr;
    }

    inline virtual SciQLopHistogram* bars(const SciQLopPyBuffer& edges,
                                          const SciQLopPyBuffer& heights,
                                          QString name = "Bars",
                                          QVariantMap metadata = {})
    {
        report_unsupported_call("bars", typeid(*this).name());
        return nullptr;
    }

    inline virtual SciQLopPlottableInterface* plottable(int index = -1)
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    inline virtual SciQLopPlottableInterface* plottable(const QString& name)
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    //! Deprecated: remove(). A plottable of another plot raises, a null one does nothing.
    virtual void remove_plottable(SciQLopPlottableInterface* plottable)
    {
        if (plottable)
            remove(plottable);
    }

    virtual void remove_plottable(int index)
    {
        SCIQLOP_ON_OWNER_THREAD(remove_plottable(index));
        delete plottable(index);
    }

    virtual void remove_plottable(const QString& name)
    {
        SCIQLOP_ON_OWNER_THREAD(remove_plottable(name));
        delete plottable(name);
    }

    //! Item factories: the item belongs to this plot (QObject child, C++-owned in Python) until
    //! remove() or the plot's death.
    inline virtual SciQLopTextItem* text(const QString& text, const QPointF& position, bool read_only = true, Coordinates coordinates = Coordinates::Pixels)
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    inline virtual SciQLopPixmapItem* pixmap(const QPixmap& pixmap, const QRectF& rect, bool read_only = true, Coordinates coordinates = Coordinates::Pixels)
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    inline virtual SciQLopEllipseItem* ellipse(const QRectF& rect, bool read_only = true, Coordinates coordinates = Coordinates::Pixels, const QString& tooltip = "")
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    inline virtual SciQLopEllipseItem* ellipse(const QRectF& rect, const QPen& pen, const QBrush& brush, bool read_only = true, Coordinates coordinates = Coordinates::Pixels, const QString& tooltip = "")
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    inline virtual SciQLopEllipseItem* ellipse(const QRectF& rect, const QColor& line_color, qreal line_width, const QColor& fill_color, bool read_only = true, Coordinates coordinates = Coordinates::Pixels, const QString& tooltip = "")
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    inline virtual SciQLopCurvedLineItem* curved_line(const QPointF& start, const QPointF& stop, LineTermination start_terminator = LineTermination::NoneTermination, LineTermination stop_terminator = LineTermination::Arrow, Coordinates coordinates = Coordinates::Pixels, const QString& tooltip = "")
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    //! span(key_range=range, ...): the defaults are span()'s.
    SciQLopSpan* vertical_span(const SciQLopPlotRange& range,
                               QColor color = QColor::fromRgb(100, 100, 100, 100),
                               bool read_only = false, bool visible = true,
                               const QString& tooltip = "",
                               Coordinates coordinates = Coordinates::Data, const QString& id = "")
    {
        return span(range, std::nullopt, color, read_only, visible, tooltip, coordinates, id);
    }

    //! span(value_range=range, ...): the defaults are span()'s.
    SciQLopSpan* horizontal_span(const SciQLopPlotRange& range,
                                 QColor color = QColor::fromRgb(100, 100, 100, 100),
                                 bool read_only = false, bool visible = true,
                                 const QString& tooltip = "",
                                 Coordinates coordinates = Coordinates::Data,
                                 const QString& id = "")
    {
        return span(std::nullopt, range, color, read_only, visible, tooltip, coordinates, id);
    }

    //! Deprecated in Python: span(key_range=..., value_range=...).
    SciQLopSpan* rectangular_span(const SciQLopPlotRange& key_range,
                                  const SciQLopPlotRange& value_range,
                                  QColor color = QColor::fromRgb(100, 100, 100, 100),
                                  bool read_only = false, bool visible = true,
                                  const QString& tooltip = "",
                                  Coordinates coordinates = Coordinates::Data,
                                  const QString& id = "")
    {
        return span(key_range, value_range, color, read_only, visible, tooltip, coordinates, id);
    }

    //! straight_line(position, Qt::Vertical, ...): the defaults are straight_line()'s.
    SciQLopStraightLine* vertical_line(double position, QColor color = QColor::fromRgb(0, 0, 0),
                                       bool read_only = true, bool visible = true,
                                       const QString& tooltip = "",
                                       Coordinates coordinates = Coordinates::Data)
    {
        return straight_line(position, Qt::Vertical, color, read_only, visible, tooltip,
                             coordinates);
    }

    //! straight_line(position, Qt::Horizontal, ...): the defaults are straight_line()'s.
    SciQLopStraightLine* horizontal_line(double position, QColor color = QColor::fromRgb(0, 0, 0),
                                         bool read_only = true, bool visible = true,
                                         const QString& tooltip = "",
                                         Coordinates coordinates = Coordinates::Data)
    {
        return straight_line(position, Qt::Horizontal, color, read_only, visible, tooltip,
                             coordinates);
    }

    //! A span over the key axis, the value axis or both: an axis given None is unbounded.
    //! Raises ValueError when both are None.
    inline virtual SciQLopSpan* span(std::optional<SciQLopPlotRange> key_range = std::nullopt,
                                     std::optional<SciQLopPlotRange> value_range = std::nullopt,
                                     QColor color = QColor::fromRgb(100, 100, 100, 100),
                                     bool read_only = false, bool visible = true,
                                     const QString& tooltip = "",
                                     Coordinates coordinates = Coordinates::Data,
                                     const QString& id = "")
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }
    //! A straight line at \a position: vertical (at a key) or horizontal (at a value).
    //! Read-only unless read_only=False.
    inline virtual SciQLopStraightLine* straight_line(double position, Qt::Orientation orientation,
                                                      QColor color = QColor::fromRgb(0, 0, 0),
                                                      bool read_only = true, bool visible = true,
                                                      const QString& tooltip = "",
                                                      Coordinates coordinates = Coordinates::Data)
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }

    //! One legend row for \a members, a SciQLopPlotGroup owned by this plot. Raises ValueError
    //! for a member of another plot.
    SciQLopPlotGroup* group(const QString& name,
                            const QList<SciQLopPlottableInterface*>& members = {});
    QList<SciQLopPlotGroup*> groups() const;
    //! Deletes \a group. Its members stay, with their own legend rows back.
    void remove_group(SciQLopPlotGroup* group);
    //! Deletes \a x now (or on the plot's thread). Raises ValueError for None, an object of
    //! another plot, or a part of a panel-wide object.
    void remove(SciQLopPlottableInterface* plottable);
    void remove(SciQLopPlotGroup* group);
    void remove(SciQLopItemInterface* item);
    void remove(SciQLopStraightLine* line);
    void remove(SciQLopSpan* span);

    inline bool selected() const noexcept { return m_selected; }

    void set_selected(bool selected) noexcept;

    void apply_selection_style();

    Q_SLOT virtual void deselect_all() { set_selected(false); }

    //! Toggles visibility of the currently-selected objects. Formerly the
    //! hard-coded 'H' shortcut; now a plain method a host application can bind.
    inline virtual void toggle_selected_objects_visibility() noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual QList<SciQLopPlottableInterface*> plottables() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual QList<QColor> color_palette() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual void set_color_palette(const QList<QColor>& colors) noexcept
    {
        WARN_ABSTRACT_METHOD;
    }

    inline virtual void set_auto_scale(bool auto_scale) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual bool auto_scale() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual bool save(const QString& filename, int width = 0, int height = 0,
                      double scale = 1.0, int quality = -1, const QString& format = {})
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual bool save_pdf(const QString& filename, int width = 0, int height = 0)
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual bool save_svg(const QString& filename, int width = 0, int height = 0)
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual bool save_png(const QString& filename, int width = 0, int height = 0,
                          double scale = 1.0, int quality = -1)
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual bool save_jpg(const QString& filename, int width = 0, int height = 0,
                          double scale = 1.0, int quality = -1)
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual bool save_bmp(const QString& filename, int width = 0, int height = 0,
                          double scale = 1.0)
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    void add_inspector_extension(InspectorExtension* extension)
    {
        if (m_extension_holder->add(extension))
            Q_EMIT inspector_extension_added(extension);
    }

    void remove_inspector_extension(InspectorExtension* extension)
    {
        if (m_extension_holder->remove(extension))
            Q_EMIT inspector_extension_removed(extension);
    }

    QList<InspectorExtension*> inspector_extensions() const
    {
        return m_extension_holder->list();
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void scroll_factor_changed(double factor);
    Q_SIGNAL void crosshair_enabled_changed(bool enabled);
    Q_SIGNAL void equal_aspect_ratio_changed(bool enabled);
    Q_SIGNAL void x_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void x2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void z_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void time_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void selection_changed(bool selected);
    Q_SIGNAL void name_changed(const QString& name);
    Q_SIGNAL void auto_scale_changed(bool auto_scale);
    Q_SIGNAL void graph_list_changed();
    Q_SIGNAL void resized();
    Q_SIGNAL void cursor_time_changed(double time);
    Q_SIGNAL void axis_synchronized_changed(AxisType axis, bool synchronized);
    Q_SIGNAL void inspector_extensions_changed();
    Q_SIGNAL void inspector_extension_added(InspectorExtension* extension);
    Q_SIGNAL void inspector_extension_removed(InspectorExtension* extension);

protected:
    inline virtual QList<SciQLopPlotAxisInterface*> axes_to_rescale() const noexcept
    {
        return m_axes_to_rescale;
    }

    inline virtual void set_axes_to_rescale(QList<SciQLopPlotAxisInterface*>&& axes)
    {
        m_axes_to_rescale = axes;
    }

    inline void freeze_axis(SciQLopPlotAxisInterface* axis) noexcept { m_frozen_axes.append(axis); }

    inline virtual QList<SciQLopPlotAxisInterface*> selected_axes() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return {};
    }

    inline virtual SciQLopPlotAxisInterface* axis_at(const QPointF& pos) const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }
};

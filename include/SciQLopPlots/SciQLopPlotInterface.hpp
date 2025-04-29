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
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"
#include "SciQLopPlots/unique_names_factory.hpp"
#include <QFrame>
#include <QPointF>
#include <QShortcut>
#include <QUuid>
#include <qcustomplot.h>

class SciQLopPlotInterface : public QFrame
{

    Q_OBJECT


protected:
    QList<SciQLopPlotAxisInterface*> m_axes_to_rescale;
    QList<SciQLopPlotAxisInterface*> m_frozen_axes;
    QUuid m_uuid;
    bool m_selected = false;

    inline virtual SciQLopGraphInterface*
    plot_impl(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual SciQLopGraphInterface*
    plot_impl(const QList<PyBuffer>& values, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual SciQLopColorMapInterface* plot_impl(const PyBuffer& x, const PyBuffer& y,
                                                       const PyBuffer& z,
                                                       QString name = QStringLiteral("ColorMap"),
                                                       bool y_log_scale = false,
                                                       bool z_log_scale = false, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual SciQLopGraphInterface*
    plot_impl(GetDataPyCallable callable, QStringList labels = QStringList(),
              QList<QColor> colors = QList<QColor>(), ::GraphType graph_type = ::GraphType::Line,
              ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
              QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual SciQLopColorMapInterface*
    plot_impl(GetDataPyCallable callable, QString name = QStringLiteral("ColorMap"),
              bool y_log_scale = false, bool z_log_scale = false, QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        throw std::runtime_error("Not implemented");
    }

public:
    Q_PROPERTY(bool selected READ selected WRITE set_selected NOTIFY selection_changed FINAL)

    SciQLopPlotInterface(QWidget* parent = nullptr, const QString& name = "")
            : QFrame(parent), m_uuid(QUuid::createUuid())
    {
        if (!name.isEmpty())
            setObjectName(name);
        else
            setObjectName(UniqueNamesFactory::unique_name("Plot"));

        auto auto_scale = new QShortcut(QKeySequence(Qt::Key_M), this);
        auto_scale->setContext(Qt::WidgetWithChildrenShortcut);
        connect(auto_scale, &QShortcut::activated, this, // #SciQLop-check-ignore-connect
                [this]
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
                });

        auto toggle_log = new QShortcut(QKeySequence(Qt::Key_L), this);
        toggle_log->setContext(Qt::WidgetWithChildrenShortcut);
        connect(toggle_log, &QShortcut::activated, this, // #SciQLop-check-ignore-connect
                [this]
                {
                    QList<SciQLopPlotAxisInterface*> axes;
                    if (auto ax = axis_at(mapFromGlobal(QCursor::pos())))
                        axes.append(ax);
                    else
                        axes = selected_axes();
                    for (auto ax : axes)
                        ax->set_log(not ax->log());
                });

        auto toggle_visibility = new QShortcut(QKeySequence(Qt::Key_H), this);
        toggle_visibility->setContext(Qt::WidgetWithChildrenShortcut);
        connect(toggle_visibility, &QShortcut::activated, this, // #SciQLop-check-ignore-connect
                &SciQLopPlotInterface::toggle_selected_objects_visibility);
    }

    virtual ~SciQLopPlotInterface() = default;

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
            default:
                return nullptr;
        }
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

    inline virtual SciQLopPlotLegendInterface* legend() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    inline virtual void set_scroll_factor(double factor) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual double scroll_factor() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return std::nan("");
    }

    inline virtual void enable_cursor(bool enable = true) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void enable_legend(bool show = true) noexcept { WARN_ABSTRACT_METHOD; }

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

    inline virtual SciQLopGraphInterface*
    line(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
         QList<QColor> colors = QList<QColor>(),
         ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, QVariantMap metaData={})
    {
        return plot_impl(x, y, labels, colors, ::GraphType::Line, marker);
    }

    inline virtual SciQLopGraphInterface*
    scatter(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
            QList<QColor> colors = QList<QColor>(),
            ::GraphMarkerShape marker = ::GraphMarkerShape::Cross, QVariantMap metaData={})
    {
        return plot_impl(x, y, labels, colors, ::GraphType::Scatter, marker, metaData);
    }

    inline virtual SciQLopGraphInterface*
    parametric_curve(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, QVariantMap metaData={})
    {
        return plot_impl(x, y, labels, colors, ::GraphType::ParametricCurve, marker, metaData);
    }

    inline virtual SciQLopGraphInterface*
    parametric_curve(const QList<PyBuffer>& values, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, QVariantMap metaData={})
    {
        return plot_impl(values, labels, colors, ::GraphType::ParametricCurve, marker,metaData);
    }

    inline virtual SciQLopColorMapInterface* colormap(const PyBuffer& x, const PyBuffer& y,
                                                      const PyBuffer& z,
                                                      QString name = QStringLiteral("ColorMap"),
                                                      bool y_log_scale = false,
                                                      bool z_log_scale = false, QVariantMap metaData={})
    {
        return plot_impl(x, y, z, name, y_log_scale, z_log_scale,metaData);
    }

    inline virtual SciQLopGraphInterface*
    line(GetDataPyCallable callable, QStringList labels = QStringList(),
         QList<QColor> colors = QList<QColor>(),
         ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker, QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        return plot_impl(callable, labels, colors, ::GraphType::Line, marker, sync_with,metaData);
    }

    inline virtual SciQLopGraphInterface*
    scatter(GetDataPyCallable callable, QStringList labels = QStringList(),
            QList<QColor> colors = QList<QColor>(),
            GraphMarkerShape marker = ::GraphMarkerShape::Cross, QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        return plot_impl(callable, labels, colors, ::GraphType::Scatter, marker, sync_with,metaData);
    }

    inline virtual SciQLopGraphInterface*
    parametric_curve(GetDataPyCallable callable, QStringList labels = QStringList(),
                     QList<QColor> colors = QList<QColor>(),
                     ::GraphMarkerShape marker = ::GraphMarkerShape::NoMarker,
                     QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        return plot_impl(callable, labels, colors, ::GraphType::ParametricCurve, marker, sync_with,metaData);
    }

    inline virtual SciQLopColorMapInterface*
    colormap(GetDataPyCallable callable, QString name = QStringLiteral("ColorMap"),
             bool y_log_scale = false, bool z_log_scale = false, QObject* sync_with = nullptr, QVariantMap metaData={})
    {
        return plot_impl(callable, name, y_log_scale, z_log_scale, sync_with,metaData);
    }

    inline virtual SciQLopPlottableInterface* plottable(int index = -1)
    {
        throw std::runtime_error("Not implemented");
    }

    inline virtual SciQLopPlottableInterface* plottable(const QString& name)
    {
        throw std::runtime_error("Not implemented");
    }

    inline bool selected() const noexcept { return m_selected; }

    void set_selected(bool selected) noexcept;

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


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void scroll_factor_changed(double factor);
    Q_SIGNAL void x_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void x2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void z_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void y2_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void time_axis_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void selection_changed(bool selected);
    Q_SIGNAL void auto_scale_changed(bool auto_scale);
    Q_SIGNAL void graph_list_changed();
    Q_SIGNAL void resized();

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

    inline virtual void toggle_selected_objects_visibility() noexcept { WARN_ABSTRACT_METHOD; }
};

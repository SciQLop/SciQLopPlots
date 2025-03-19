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
#include "SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"
#include <QObject>
#include <QPointer>
class QCPAxis;
class QCPColorScale;


class SciQLopPlotAxisInterface : public QObject
{
    Q_OBJECT
protected:
    bool _is_time_axis = false;
public:
    SciQLopPlotAxisInterface(QObject* parent = nullptr, const QString& name = "") : QObject(parent)
    {
        if (!name.isEmpty())
            setObjectName(name);
        else
            setObjectName("Axis");
    }
    virtual ~SciQLopPlotAxisInterface() = default;

    inline virtual void set_range(const SciQLopPlotRange&) noexcept { }
    inline void set_range(double start, double stop) noexcept
    {
        set_range(SciQLopPlotRange(start, stop));
    }
    inline virtual void set_visible(bool visible) noexcept { }
    inline virtual void set_log(bool log) noexcept { }
    inline virtual void set_label(const QString& label) noexcept { }
    inline virtual void set_tick_labels_visible(bool visible) noexcept { }
    inline virtual void set_selected(bool selected) noexcept { Q_UNUSED(selected); }

    inline virtual SciQLopPlotRange range() const noexcept
    {
        return SciQLopPlotRange(std::nan(""), std::nan(""));
    }
    inline virtual bool visible() const noexcept { return false; }
    inline virtual bool log() const noexcept { return false; }
    inline virtual QString label() const noexcept { return QString(); }
    inline virtual bool tick_labels_visible() const noexcept { return false; }
    inline virtual Qt::Orientation orientation() const noexcept { return Qt::Horizontal; }
    inline virtual Qt::Axis axis() const noexcept { return Qt::XAxis; }
    inline virtual Qt::AnchorPoint anchor() const noexcept { return Qt::AnchorBottom; }

    inline virtual bool selected() const noexcept { return false; }

    inline virtual void rescale() noexcept { }

    inline bool is_time_axis() const noexcept { return _is_time_axis; }
    inline void set_is_time_axis(bool is_time_axis) noexcept { _is_time_axis = is_time_axis; }

    void couple_range_with(SciQLopPlotAxisInterface* other) noexcept;
    void decouple_range_from(SciQLopPlotAxisInterface* other) noexcept;


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange range);
    Q_SIGNAL void visible_changed(bool visible);
    Q_SIGNAL void tick_labels_visible_changed(bool visible);
    Q_SIGNAL void log_changed(bool log);
    Q_SIGNAL void label_changed(const QString& label);
    Q_SIGNAL void selection_changed(bool selected);

};


class SciQLopPlotDummyAxis : public SciQLopPlotAxisInterface
{
    Q_OBJECT
    SciQLopPlotRange m_range;

public:
    explicit SciQLopPlotDummyAxis(QObject* parent = nullptr) : SciQLopPlotAxisInterface(parent) { }
    virtual ~SciQLopPlotDummyAxis() = default;

    void set_range(const SciQLopPlotRange& range) noexcept override;
    inline SciQLopPlotRange range() const noexcept override { return m_range; }
};

class SciQLopPlotAxis : public SciQLopPlotAxisInterface
{
    Q_OBJECT
    QPointer<QCPAxis> m_axis;

public:
    explicit SciQLopPlotAxis(QCPAxis* axis, QObject* parent = nullptr, bool is_time_axis = false, const QString &name="Axis");
    virtual ~SciQLopPlotAxis() = default;

    void set_range(const SciQLopPlotRange& range) noexcept override;
    void set_visible(bool visible) noexcept override;
    void set_log(bool log) noexcept override;
    void set_label(const QString& label) noexcept override;
    void set_tick_labels_visible(bool visible) noexcept override;
    void set_selected(bool selected) noexcept override;
    SciQLopPlotRange range() const noexcept override;
    bool visible() const noexcept override;
    bool log() const noexcept override;
    QString label() const noexcept override;
    bool tick_labels_visible() const noexcept override;
    Qt::Orientation orientation() const noexcept override;
    Qt::Axis axis() const noexcept override;
    Qt::AnchorPoint anchor() const noexcept override;
    bool selected() const noexcept override;

    void rescale() noexcept override;

    virtual QCPAxis* qcp_axis() const noexcept;
};

class SciQLopPlotColorScaleAxis : public SciQLopPlotAxis
{
    Q_OBJECT
    QPointer<QCPColorScale> m_axis;
    ColorGradient m_color_gradient;

public:
    explicit SciQLopPlotColorScaleAxis(QCPColorScale* axis, QObject* parent = nullptr, const QString &name="ColorScale");
    virtual ~SciQLopPlotColorScaleAxis() = default;

    void set_range(const SciQLopPlotRange& range) noexcept override;
    void set_visible(bool visible) noexcept override;
    void set_log(bool log) noexcept override;
    void set_label(const QString& label) noexcept override;
    void set_color_gradient(const ColorGradient gradient) noexcept;

    SciQLopPlotRange range() const noexcept override;
    bool visible() const noexcept override;
    bool log() const noexcept override;
    QString label() const noexcept override;
    ColorGradient color_gradient() const noexcept;
    Qt::Orientation orientation() const noexcept override;
    Qt::Axis axis() const noexcept override;
    Qt::AnchorPoint anchor() const noexcept override;
    bool selected() const noexcept override;

    void rescale() noexcept override;


    QCPAxis* qcp_axis() const noexcept override;
    QCPColorScale* qcp_colorscale() const noexcept;


#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void color_gradient_changed(ColorGradient gradient);

};

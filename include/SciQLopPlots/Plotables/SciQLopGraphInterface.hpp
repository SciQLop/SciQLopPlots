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

#include "SciQLopPlots/Python/PythonInterface.hpp"

#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"

#include <QColor>
#include <QList>
#include <QObject>
#include <utility>

class SciQLopPlottableInterface : public QObject
{
    Q_OBJECT

protected:
    SciQLopPlotRange m_range;

public:
    Q_PROPERTY(bool selected READ selected WRITE set_selected NOTIFY selection_changed)

    SciQLopPlottableInterface(QObject* parent = nullptr) : QObject(parent) { }

    virtual ~SciQLopPlottableInterface() = default;

    virtual void set_range(const SciQLopPlotRange& range);

    virtual void set_visible(bool visible) noexcept { WARN_ABSTRACT_METHOD; }

    inline virtual void set_name(const QString& name) noexcept { this->setObjectName(name); }

    virtual SciQLopPlotRange range() const noexcept { return m_range; }

    virtual bool visible() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual QString name() const noexcept { return this->objectName(); }

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) { WARN_ABSTRACT_METHOD; };

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z) { WARN_ABSTRACT_METHOD; };

    virtual QList<PyBuffer> data() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QList<PyBuffer>();
    }

    virtual void set_selected(bool selected) noexcept { WARN_ABSTRACT_METHOD; }

    virtual bool selected() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(SciQLopPlotRange range);
    Q_SIGNAL void visible_changed(bool visible);
    Q_SIGNAL void name_changed(const QString& name);
    Q_SIGNAL void replot();
    Q_SIGNAL void data_changed(PyBuffer x, PyBuffer y);
    Q_SIGNAL void data_changed(PyBuffer x, PyBuffer y, PyBuffer z);
    Q_SIGNAL void selection_changed(bool selected);
#endif
};

class SciQLopGraphInterface : public SciQLopPlottableInterface
{
    Q_OBJECT

public:
    Q_PROPERTY(bool selected READ selected WRITE set_selected NOTIFY selection_changed)

    SciQLopGraphInterface(QObject* parent = nullptr);
    virtual ~SciQLopGraphInterface() = default;

    virtual void set_labels(const QStringList& labels) { WARN_ABSTRACT_METHOD; }

    virtual void set_colors(const QList<QColor>& colors) { WARN_ABSTRACT_METHOD; }

    virtual QStringList labels() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QStringList();
    }

    virtual QList<QColor> colors() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QList<QColor>();
    }

    virtual SciQLopGraphComponentInterface* component(const QString& name) const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    virtual SciQLopGraphComponentInterface* component(int index) const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return nullptr;
    }

    virtual QList<SciQLopGraphComponentInterface*> components() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QList<SciQLopGraphComponentInterface*>();
    }

#ifndef BINDINGS_H
    Q_SIGNAL void labels_changed(const QStringList& labels);
    Q_SIGNAL void colors_changed(const QList<QColor>& colors);
    Q_SIGNAL void component_list_changed();
#endif
};

class SciQLopColorMapInterface : public SciQLopPlottableInterface
{
    Q_OBJECT

public:
    SciQLopColorMapInterface(QObject* parent = nullptr);
    virtual ~SciQLopColorMapInterface() = default;

    virtual void set_y_log_scale(bool y_log_scale) noexcept { WARN_ABSTRACT_METHOD; }

    virtual void set_z_log_scale(bool z_log_scale) noexcept { WARN_ABSTRACT_METHOD; }

    virtual bool y_log_scale() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual bool z_log_scale() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }

    virtual void set_selected(bool selected) noexcept { WARN_ABSTRACT_METHOD; }
};

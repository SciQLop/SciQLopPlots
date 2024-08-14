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

#include <QColor>
#include <QList>
#include <QObject>
#include <utility>

class SciQLopGraphInterface : public QObject
{
    std::pair<double, double> m_range = { std::nan(""), std::nan("") };
    Q_OBJECT
public:
    SciQLopGraphInterface(QObject* parent = nullptr);
    virtual ~SciQLopGraphInterface() = default;

    virtual void set_range(double lower, double upper);
    virtual void set_visible(bool visible) noexcept { }
    virtual void set_labels(const QStringList& labels) { }
    inline virtual void set_name(const QString& name) noexcept { this->setObjectName(name); }
    virtual void set_colors(const QList<QColor>& colors) { }

    virtual std::pair<double, double> range() const noexcept { return m_range; }

    virtual bool visible() const noexcept { return false; }
    virtual QStringList labels() const noexcept { return QStringList(); }
    virtual QString name() const noexcept { return this->objectName(); }
    virtual QList<QColor> colors() const noexcept { return QList<QColor>(); }

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) {};
    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z) {};

    virtual QList<PyBuffer> data() const noexcept { return QList<PyBuffer>(); }

#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(double lower, double upper);
    Q_SIGNAL void visible_changed(bool visible);
    Q_SIGNAL void labels_changed(const QStringList& labels);
    Q_SIGNAL void name_changed(const QString& name);
    Q_SIGNAL void colors_changed(const QList<QColor>& colors);
    Q_SIGNAL void replot();
    Q_SIGNAL void data_changed(PyBuffer x, PyBuffer y);
    Q_SIGNAL void data_changed(PyBuffer x, PyBuffer y, PyBuffer z);
#endif
};

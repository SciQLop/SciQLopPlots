/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2020, Plasma Physics Laboratory - CNRS
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

#include <QLayout>
#include <QObject>
#include <QWidget>

#include <cmath>
#include <utility>

namespace SciQLopPlots
{

/*
 * expected feature list:
 * - mouse wheel scroll/zoom
 * - box zoom
 * - mouse drag scroll
 * - key scroll/zoom
 * - multi-graph sync
 * - graph dragn'grop
 * - data tooltip
 * - basic plot customization, color/style/scales
 * - x axis sharing and x axis sticky
 * - time interval selection boxes
 */

inline void removeAllMargins(QWidget* widget)
{
    widget->setContentsMargins(0, 0, 0, 0);
    auto layout = widget->layout();
    if (layout)
    {
        layout->setSpacing(0);
        layout->setMargin(0);
        layout->setContentsMargins(0, 0, 0, 0);
    }
}

struct AxisRange : std::pair<double, double>
{
    AxisRange(double lower, double upper) : std::pair<double, double> { lower, upper } { }

    double center() const noexcept { return (second + first) / 2.; }
    double width() const noexcept { return second - first; }

    AxisRange& operator*(double factor)
    {
        auto _center = center();
        auto newHalfWidth = width() * factor / 2;
        first = _center - newHalfWidth;
        second = _center + newHalfWidth;
        return *this;
    }

    AxisRange& operator+(double offset)
    {
        first += offset;
        second += offset;
        return *this;
    }

    AxisRange& operator-(double offset)
    {
        first -= offset;
        second -= offset;
        return *this;
    }
};

class IPlotWidget : public QWidget
{
    Q_OBJECT
public:
    IPlotWidget(QWidget* parent = nullptr) : QWidget(parent) { }
    virtual void zoom(double factor, Qt::Orientation orientation = Qt::Horizontal) = 0;
    virtual void zoom(double factor, QPoint center, Qt::Orientation orientation = Qt::Horizontal)
        = 0;
    virtual void move(double factor, Qt::Orientation orientation) = 0;
    virtual void move(double dx, double dy) = 0;

    virtual void autoScaleY() = 0;

    virtual int addGraph(QColor color = Qt::blue) = 0;

    virtual void setXRange(const AxisRange& range) = 0;
    virtual void setYRange(const AxisRange& range) = 0;

    virtual void showXAxis(bool show) = 0;

    Q_SIGNAL void xRangeChanged(AxisRange newRange);
    Q_SIGNAL void yRangeChanged(AxisRange newRange);
    Q_SIGNAL void dataChanged();
    Q_SIGNAL void closed();

protected:
    void setWidget(QWidget* widget);
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    /*void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;*/
private:
    // bool m_mousePressed = false;
    std::optional<QPoint> m_lastMousePress = std::nullopt;
};

}
